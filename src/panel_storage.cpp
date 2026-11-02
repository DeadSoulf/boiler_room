// microSD for GUITION JC4880P443 / ESP32-P4.
// Display BSP already powers the TF slot from LDO channel 4 (3.3V).
// Card uses SDMMC SLOT 0 (GPIO43/44/39/40/41/42). C6 uses SLOT 1.
// No automatic formatting at boot. Web requests must be physically approved.
// Dedicated SD worker owns mounting, formatting and logging.
#include <Arduino.h>
#include <SD_MMC.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_err.h"
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "board_p4_pins.h"
#include "panel_storage.h"

namespace panel_storage {
namespace {
constexpr char kRoot[]="/boiler-panel";
constexpr char kLogs[]="/boiler-panel/logs";
constexpr char kEvents[]="/boiler-panel/logs/events.log";
constexpr char kSamples[]="/boiler-panel/logs/temperatures.csv";
constexpr uint32_t kRefreshMs=30000;
constexpr uint32_t kRetryMs=120000;
constexpr uint32_t kEventLimit=128*1024;
constexpr uint32_t kSampleLimit=256*1024;
constexpr uint64_t kMinReserve=8ULL*1024*1024;
enum class ItemKind:uint8_t { Event, Measurements };
struct Item {
  ItemKind type=ItemKind::Event;
  uint32_t at=0;
  time_t wall=0;
  char code[18]={},detail[83]={};
  float value[panel::CHANNELS]={};
  uint8_t valid[panel::CHANNELS]={};
};
Snapshot published;
QueueHandle_t pending=nullptr;
SemaphoreHandle_t stateLock=nullptr;
TaskHandle_t workerTask=nullptr;
bool mounted=false;
bool writable=false;
uint32_t lastStats=0;
uint32_t lastAttempt=0;
uint32_t formatDeadline=0;
bool formatRunning=false;
constexpr uint32_t kFormatWindowMs=90000;
constexpr char kMountPath[]="/sdcard";

void safeText(char* target,size_t cap,const char* source);
void setFormatPhase(FormatPhase phase,const char* message){
  if(!stateLock||xSemaphoreTake(stateLock,pdMS_TO_TICKS(40))!=pdTRUE)return;
  published.formatPhase=phase;
  if(message)safeText(published.formatMessage,sizeof(published.formatMessage),message);
  published.formatRemainingSeconds=0;
  xSemaphoreGive(stateLock);
}
void safeText(char* target,size_t cap,const char* source) {
  if(!target||!cap)return;
  size_t n=0;
  if(source)for(;source[n]&&n+1<cap;n++){
    const unsigned char c=static_cast<unsigned char>(source[n]);
    target[n]=(c<32||c==127||c==','||c==';'||c=='|')?' ':char(c);
  }
  target[n]=0;
}
void publishState(const char* message=nullptr) {
  if(!stateLock || xSemaphoreTake(stateLock,pdMS_TO_TICKS(30))!=pdTRUE)return;
  if(message)safeText(published.message,sizeof(published.message),message);
  published.started=true;
  published.mounted=mounted;
  published.logging=mounted&&writable;
  published.refreshedAt=millis();
  xSemaphoreGive(stateLock);
}
void failure(const char* message) {
  writable=false;
  publishState(message);
}
void countWritten(bool sample) {
  if(!stateLock||xSemaphoreTake(stateLock,pdMS_TO_TICKS(25))!=pdTRUE)return;
  if(sample)++published.writtenSamples;
  else ++published.writtenEvents;
  xSemaphoreGive(stateLock);
}
void countDropped() {
  if(!stateLock||xSemaphoreTake(stateLock,0)!=pdTRUE)return;
  ++published.dropped;
  xSemaphoreGive(stateLock);
}
void cardStats() {
  const uint64_t card=SD_MMC.cardSize();
  const uint64_t total=SD_MMC.totalBytes();
  const uint64_t used=SD_MMC.usedBytes();
  const uint64_t free=(total>used)?total-used:0;
  if(!stateLock||xSemaphoreTake(stateLock,pdMS_TO_TICKS(50))!=pdTRUE)return;
  published.cardBytes=card;
  published.totalBytes=total;
  published.usedBytes=used<=total?used:0;
  published.freeBytes=free;
  published.refreshedAt=millis();
  xSemaphoreGive(stateLock);
  if(card==0 || total==0 || used>total){
    failure("Не удалось прочитать объём файловой системы");
    return;
  }
  const uint64_t reserve=(total/20>kMinReserve)?total/20:kMinReserve;
  if(free<=reserve){
    failure("Недостаточно свободного места для журналов");
    return;
  }
  writable=true;
  publishState("Карта доступна; журналы ограничены по размеру");
}
bool makeDirectories() {
  if(!SD_MMC.exists(kRoot) && !SD_MMC.mkdir(kRoot))return false;
  if(!SD_MMC.exists(kLogs) && !SD_MMC.mkdir(kLogs))return false;
  return true;
}
bool mountCard() {
#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  // Arduino's P4 variant defaults to BOARD_SDMMC_POWER_CHANNEL=4, but
  // board_p4_display_init() has ALREADY acquired LDO4 for TF_VCC.
  // Never re-acquire LDO4 in SD_MMC or the SD mount fails.
  if(!SD_MMC.setPowerChannel(-1)){
    mounted=false;failure("Не удалось отключить повторное управление LDO4");return false;
  }
#endif
  if(!SD_MMC.setPins(BOARD_P4_SD_CLK,BOARD_P4_SD_CMD,BOARD_P4_SD_D0,
                     BOARD_P4_SD_D1,BOARD_P4_SD_D2,BOARD_P4_SD_D3)){
    mounted=false;failure("Ошибка настройки SDMMC GPIO");return false;
  }
  // All normal mounts are non-destructive; native formatting is separate.
  if(!SD_MMC.begin(kMountPath,false,false,SDMMC_FREQ_DEFAULT,3)){
    mounted=false;failure("Нет карты или неподдерживаемая файловая система");
    Serial.println("[BP-SD] regular mount failed; NO automatic format");
    return false;
  }
  mounted=true;
  writable=false;
  cardStats();
  if(!makeDirectories()){
    failure("Карта смонтирована, папка журналов недоступна");
  } else {
    cardStats();
  }
  const Snapshot now=[](){
    Snapshot result{};
    if(stateLock && xSemaphoreTake(stateLock,pdMS_TO_TICKS(50))==pdTRUE){
      result=published;xSemaphoreGive(stateLock);
    }
    return result;
  }();
  Serial.printf("[BP-SD] mounted card=%llu bytes fs=%llu used=%llu free=%llu logs=%s\n",
    (unsigned long long)now.cardBytes,(unsigned long long)now.totalBytes,
    (unsigned long long)now.usedBytes,(unsigned long long)now.freeBytes,
    now.logging?"enabled":"disabled");
  return true;
}
bool rotate(const char* current,uint32_t maxSize) {
  if(!SD_MMC.exists(current))return true;
  File handle=SD_MMC.open(current,FILE_READ);
  if(!handle)return false;
  const uint32_t bytes=uint32_t(handle.size());
  handle.close();
  if(bytes<maxSize)return true;
  char previous[100],older[100];
  snprintf(previous,sizeof(previous),"%s.1",current);
  snprintf(older,sizeof(older),"%s.2",current);
  // Only these two exact application-owned files are ever removed/renamed.
  if(SD_MMC.exists(older) && !SD_MMC.remove(older))return false;
  if(SD_MMC.exists(previous) && !SD_MMC.rename(previous,older))return false;
  return SD_MMC.rename(current,previous);
}
bool appendLine(const char* path,const char* text,size_t length,uint32_t maxSize) {
  if(!mounted||!writable||!text||!length)return false;
  if(!rotate(path,maxSize))return false;
  File file=SD_MMC.open(path,FILE_APPEND);
  if(!file)return false;
  const size_t n=file.write(reinterpret_cast<const uint8_t*>(text),length);
  file.flush();
  file.close();
  return n==length;
}
void makePrefix(char* dst,size_t cap,const Item& entry) {
  if(entry.wall>=time_t(1704067200)) {
    tm local{};
    localtime_r(&entry.wall,&local);
    strftime(dst,cap,"%Y-%m-%d %H:%M:%S",&local);
  } else snprintf(dst,cap,"uptime=%lu",(unsigned long)entry.at);
}
bool writeEvent(const Item& entry) {
  char stamp[32],line[220];
  makePrefix(stamp,sizeof(stamp),entry);
  const int n=snprintf(line,sizeof(line),"%s | %s | %s\n",stamp,entry.code,entry.detail);
  if(n<0||size_t(n)>=sizeof(line))return false;
  return appendLine(kEvents,line,size_t(n),kEventLimit);
}
bool writeMeasurements(const Item& entry) {
  char stamp[32],line[180];makePrefix(stamp,sizeof(stamp),entry);
  int n=snprintf(line,sizeof(line),"%s;%lu",stamp,(unsigned long)entry.at);
  if(n<0||size_t(n)>=sizeof(line))return false;
  for(unsigned i=0;i<panel::CHANNELS;i++) {
    const int m=entry.valid[i]?
      snprintf(line+n,sizeof(line)-size_t(n),";%.2f",double(entry.value[i])):
      snprintf(line+n,sizeof(line)-size_t(n),";");
    if(m<0||size_t(m)>=sizeof(line)-size_t(n))return false;
    n+=m;
  }
  if(size_t(n)+1>=sizeof(line))return false;
  line[n++]='\n';line[n]=0;
  return appendLine(kSamples,line,size_t(n),kSampleLimit);
}


// Destructive FAT preparation, called ONLY on the SD worker, after both
// authenticated browser request AND a physical touchscreen confirmation.
// For this board the BSP owns LDO4. The SD controller uses SDMMC SLOT 0.
bool formatPhysicalCard(){
  // Stop all prior SD_MMC users; only this worker accesses the filesystem.
  writable=false;
  SD_MMC.end();
  mounted=false;
  publishState("Подготовка microSD к форматированию");
  sdmmc_host_t host=SDMMC_HOST_DEFAULT();
  host.slot=SDMMC_HOST_SLOT_0;
  host.flags=SDMMC_HOST_FLAG_4BIT;
  host.max_freq_khz=SDMMC_FREQ_DEFAULT;
  sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT();
  // ESP32-P4 slot 0 is hardware IO_MUX, not a GPIO matrix bus.
  // Use zeroed pin fields exactly as the Arduino SD_MMC Slot 0 wrapper does.
  slot.clk=GPIO_NUM_0;
  slot.cmd=GPIO_NUM_0;
  slot.d0=GPIO_NUM_0;
  slot.d1=GPIO_NUM_0;
  slot.d2=GPIO_NUM_0;
  slot.d3=GPIO_NUM_0;
  slot.d4=GPIO_NUM_0;
  slot.d5=GPIO_NUM_0;
  slot.d6=GPIO_NUM_0;
  slot.d7=GPIO_NUM_0;
  slot.cd=SDMMC_SLOT_NO_CD;
  slot.wp=SDMMC_SLOT_NO_WP;
  slot.width=4;
  slot.flags=0;
  esp_vfs_fat_mount_config_t cfg={};
  // This is intentionally true ONLY within explicit physical confirmation.
  // On exFAT/no-FAT this partitions+creates FAT, on existing FAT it mounts.
  cfg.format_if_mount_failed=true;
  cfg.max_files=3;
  cfg.allocation_unit_size=0;
  cfg.use_one_fat=false;
  sdmmc_card_t* card=nullptr;
  Serial.println("[BP-SD] WEB+PHYSICAL CONFIRMED: mount SDMMC0 for FAT formatting");
  esp_err_t err=esp_vfs_fat_sdmmc_mount(kMountPath,&host,&slot,&cfg,&card);
  if(err!=ESP_OK||!card){
    Serial.printf("[BP-SD] FAT prepare mount failed: %s (0x%x)\n",esp_err_to_name(err),unsigned(err));
    failure("Ошибка инициализации носителя для форматирования");
    return false;
  }
  // Always format on explicit command, including a previously valid FAT card.
  // This API only targets the SD card handle returned for SDMMC0 above.
  err=esp_vfs_fat_sdcard_format(kMountPath,card);
  if(err!=ESP_OK){
    Serial.printf("[BP-SD] FAT format failed: %s (0x%x)\n",esp_err_to_name(err),unsigned(err));
    failure("Ошибка форматирования FAT; перезапустите панель");
    // IDF might have recycled card resources after a failed remount.
    // Do not reuse or double-free the card; do not retry automatically.
    return false;
  }
  err=esp_vfs_fat_sdcard_unmount(kMountPath,card);
  if(err!=ESP_OK){
    Serial.printf("[BP-SD] FAT unmount failed: 0x%x\n",unsigned(err));
    failure("Ошибка размонтирования после форматирования");
    return false;
  }
  return true;
}
void runPhysicalFormat(){
  // xQueueReset discards pending old log events before the card is replaced.
  if(pending)xQueueReset(pending);
  setFormatPhase(FormatPhase::Running,"Разметка и создание FAT; не отключайте питание");
  const bool formatted=formatPhysicalCard();
  if(!formatted){
    setFormatPhase(FormatPhase::Failed,"Не удалось отформатировать microSD. Данные могут быть удалены.");
    return;
  }
  setFormatPhase(FormatPhase::Verifying,"Проверка FAT и пробная запись журнала");
  const bool mountedOK=mountCard();
  if(!mountedOK||!mounted||!writable){
    setFormatPhase(FormatPhase::Failed,"FAT создана, но повторное подключение или проверка места не прошла");
    return;
  }
  Item sample{};
  sample.type=ItemKind::Event;
  sample.at=millis();
  sample.wall=time(nullptr);
  safeText(sample.code,sizeof(sample.code),"storage");
  safeText(sample.detail,sizeof(sample.detail),"FAT prepared after physical confirmation");
  const bool written=writeEvent(sample);
  if(written)countWritten(false);
  File check=SD_MMC.open(kEvents,FILE_READ);
  const bool verified=written && check && check.size()>0;
  if(check)check.close();
  cardStats();
  if(!verified || !writable){
    failure("Пробная запись/чтение microSD не прошла");
    setFormatPhase(FormatPhase::Failed,"FAT создана, но проверка журнала не прошла");
    return;
  }
  setFormatPhase(FormatPhase::Complete,"FAT готова. Запись и чтение журнала проверены.");
  Serial.println("[BP-SD] FAT format + read/write verification SUCCESS");
}
void worker(void*) {
  // The UI first frame and Wi-Fi initialization must not wait for SD card I/O.
  vTaskDelay(pdMS_TO_TICKS(3500));
  lastAttempt=millis();
  mountCard();
  lastStats=millis();
  if(mounted&&writable){
    Item startup;startup.type=ItemKind::Event;startup.at=millis();startup.wall=time(nullptr);
    safeText(startup.code,sizeof(startup.code),"storage");
    safeText(startup.detail,sizeof(startup.detail),"Panel started; microSD mounted");
    if(writeEvent(startup))countWritten(false);
    else{failure("Ошибка записи первого события");countDropped();}
  }
  for(;;) {
    bool startFormat=false;
    if(stateLock && xSemaphoreTake(stateLock,0)==pdTRUE){
      if(published.formatPhase==FormatPhase::AwaitingPhysical && 
         int32_t(millis()-formatDeadline)>=0){
        published.formatPhase=FormatPhase::Expired;
        published.formatRemainingSeconds=0;
        safeText(published.formatMessage,sizeof(published.formatMessage),
                 "Время физического подтверждения истекло");
      }
      if(published.formatPhase==FormatPhase::Queued && !published.formatAttempted){
        published.formatPhase=FormatPhase::Running;
        published.formatAttempted=true;
        safeText(published.formatMessage,sizeof(published.formatMessage),
                 "Начато форматирование microSD");
        startFormat=true;
      }
      xSemaphoreGive(stateLock);
    }
    if(startFormat){runPhysicalFormat();continue;}
    const uint32_t now=millis();
    if(!mounted && uint32_t(now-lastAttempt)>=kRetryMs){
      lastAttempt=now;
      SD_MMC.end();
      mountCard();
    }
    if(mounted && uint32_t(now-lastStats)>=kRefreshMs){
      lastStats=now;cardStats();
    }
    Item entry;
    if(pending && xQueueReceive(pending,&entry,pdMS_TO_TICKS(300))==pdTRUE){
      if(!mounted||!writable){countDropped();continue;}
      const bool sample=entry.type==ItemKind::Measurements;
      const bool ok=sample?writeMeasurements(entry):writeEvent(entry);
      if(ok)countWritten(sample);
      else {failure("Ошибка записи журнала");countDropped();}
    }
  }
}
} // namespace

bool begin(){
  if(workerTask)return true;
  stateLock=xSemaphoreCreateMutex();
  pending=xQueueCreate(20,sizeof(Item));
  if(!stateLock||!pending)return false;
  if(xTaskCreate(worker,"panel-sd",8192,nullptr,1,&workerTask)!=pdPASS){
    workerTask=nullptr;return false;
  }
  publishState("Проверка microSD...");
  Serial.println("[BP-SD] microSD init; automatic formatting DISABLED");
  return true;
}
bool snapshot(Snapshot& out){
  if(!stateLock || xSemaphoreTake(stateLock,0)!=pdTRUE)return false;
  out=published;
  if(out.formatPhase==FormatPhase::AwaitingPhysical && int32_t(formatDeadline-millis())>0)
    out.formatRemainingSeconds=(uint32_t(formatDeadline-millis())+999U)/1000U;
  else out.formatRemainingSeconds=0;
  xSemaphoreGive(stateLock);return true;
}
bool event(const char* category,const char* description){
  if(!pending)return false;
  Item e;
  e.type=ItemKind::Event;e.at=millis();e.wall=time(nullptr);
  safeText(e.code,sizeof(e.code),category);
  safeText(e.detail,sizeof(e.detail),description);
  if(xQueueSend(pending,&e,0)==pdTRUE)return true;
  countDropped();return false;
}
bool measurements(const panel::Measurement (&temps)[panel::CHANNELS],uint32_t atMs){
  if(!pending)return false;
  static uint32_t lastSampleAt=0;
  if(lastSampleAt && uint32_t(atMs-lastSampleAt)<60000U)return false;
  Item e;e.type=ItemKind::Measurements;e.at=atMs;e.wall=time(nullptr);
  bool any=false;
  for(unsigned i=0;i<panel::CHANNELS;i++){
    if(temps[i].fresh(atMs)){
      e.valid[i]=1;
      e.value[i]=temps[i].value;
      any=true;
    }
  }
  if(!any)return false;
  lastSampleAt=atMs;
  if(xQueueSend(pending,&e,0)==pdTRUE)return true;
  countDropped();return false;
}


bool prepareFormat(){
  if(!stateLock||!workerTask)return false;
  if(xSemaphoreTake(stateLock,pdMS_TO_TICKS(30))!=pdTRUE)return false;
  const FormatPhase phase=published.formatPhase;
  const bool available=!published.formatAttempted &&
    phase!=FormatPhase::AwaitingPhysical && phase!=FormatPhase::Queued &&
    phase!=FormatPhase::Running && phase!=FormatPhase::Verifying;
  if(available){
    formatDeadline=millis()+kFormatWindowMs;
    published.formatPhase=FormatPhase::AwaitingPhysical;
    published.formatRemainingSeconds=kFormatWindowMs/1000U;
    safeText(published.formatMessage,sizeof(published.formatMessage),
      "ПОДТВЕРДИТЕ стирание ВСЕХ данных microSD на физическом дисплее");
  }
  xSemaphoreGive(stateLock);
  return available;
}
bool confirmFormatPhysically(){
  if(!stateLock||xSemaphoreTake(stateLock,pdMS_TO_TICKS(30))!=pdTRUE)return false;
  const bool valid=published.formatPhase==FormatPhase::AwaitingPhysical &&
    !published.formatAttempted && int32_t(formatDeadline-millis())>0;
  if(valid){
    published.formatPhase=FormatPhase::Queued;
    published.formatRemainingSeconds=0;
    safeText(published.formatMessage,sizeof(published.formatMessage),
      "Подтверждено на дисплее; операция в очереди");
  }
  xSemaphoreGive(stateLock);
  return valid;
}
bool cancelFormatPhysically(){
  if(!stateLock||xSemaphoreTake(stateLock,pdMS_TO_TICKS(30))!=pdTRUE)return false;
  const bool valid=published.formatPhase==FormatPhase::AwaitingPhysical;
  if(valid){
    published.formatPhase=FormatPhase::Cancelled;
    published.formatRemainingSeconds=0;
    safeText(published.formatMessage,sizeof(published.formatMessage),
      "Запрос отменён на дисплее. Карта не изменялась");
  }
  xSemaphoreGive(stateLock);
  return valid;
}
} // namespace panel_storage
