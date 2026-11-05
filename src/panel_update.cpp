// GitHub OTA for the pinned ESP32-P4 panel. One worker owns HTTPS, OTA and NVS.
// The SD worker remains the sole filesystem owner; update events use its queue.
#include <Arduino.h>
#include <Preferences.h>
#include <sdkconfig.h>
#include <atomic>
#include <new>
#include <memory>
#include <time.h>
#include <strings.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "panel_update.h"
#include "panel_update_protocol.h"
#include "panel_radio.h"
#include "panel_storage.h"
#if __has_include("panel_update_build.h")
#include "panel_update_build.h"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32P4) && CONFIG_IDF_TARGET_ESP32P4 && \
    defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE && \
    defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) && CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#define BP_UPDATE_SUPPORTED 1
#include <esp_app_format.h>
#include <esp_chip_info.h>
#include <esp_crt_bundle.h>
#include <esp_flash.h>
#include <esp_http_client.h>
#include <esp_image_format.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>
#else
#define BP_UPDATE_SUPPORTED 0
#endif

namespace panel_update {
namespace {
SemaphoreHandle_t guard=nullptr;
QueueHandle_t requests=nullptr;
TaskHandle_t task=nullptr;
Snapshot state;
std::atomic<bool> cancelRequested{false};
std::atomic<uint32_t> heartbeatAt{0},healthySince{0};
std::atomic<bool> heartbeatReady{false};
enum class Action:uint8_t {Check,Install,Rollback};
struct Request {Action action=Action::Check;char version[24]={};};

enum Error:int32_t {
  Unsupported=-1000,Network=-1001,Clock=-1002,Memory=-1003,Http=-1004,
  Response=-1005,ManifestError=-1006,VersionError=-1007,HardwareError=-1008,
  BootloaderError=-1009,RunningState=-1010,PersistError=-1011,ImageError=-1012,
  HashError=-1013,Timeout=-1014,CancelledError=-1015,RollbackError=-1016,
  Interrupted=-1017,HealthError=-1018
};

void logEvent(const char* text){panel_storage::event("update",text);}
bool lock(uint32_t ms=25){return guard&&xSemaphoreTake(guard,pdMS_TO_TICKS(ms))==pdTRUE;}
void unlock(){xSemaphoreGive(guard);}
void phase(Phase next,const char* message,int32_t code=0){
  if(!lock())return;
  state.phase=next;state.errorCode=code;
  snprintf(state.message,sizeof(state.message),"%s",message?message:"");
  unlock();
}
void finish(Phase next,const char* message,int32_t code=0){
  if(!lock())return;
  state.phase=next;state.busy=false;state.cancelAllowed=false;state.errorCode=code;
  snprintf(state.message,sizeof(state.message),"%s",message?message:"");
  unlock();
}
bool healthy(uint32_t now){
  return heartbeatReady.load()&&uint32_t(now-heartbeatAt.load())<=policy::HEARTBEAT_MAX_GAP_MS&&
    uint32_t(now-healthySince.load())>=policy::HEALTHY_MS;
}

#if BP_UPDATE_SUPPORTED
esp_timer_handle_t bootDeadline=nullptr;
std::atomic<bool> bootConfirmed{false};
const esp_partition_t* slot0=nullptr;
const esp_partition_t* slot1=nullptr;
const esp_partition_t* running=nullptr;
bool platformReady=false;
bool pendingBoot=false;
bool pendingIdentityOK=false;
policy::Manifest offered;
policy::Release release;
bool haveOffer=false;
uint32_t workerStartedAt=0,lastAutomaticAttempt=0;
bool automaticAttempted=false;

enum class Stage:uint32_t {Empty=0,Receiving=1,Armed=2,Confirmed=3,RolledBack=4,Cancelled=5,Failed=6,Interrupted=7};
struct Record {
  uint32_t magic=0,schema=1,stage=0,manual=0;
  uint32_t fromAddress=0,targetAddress=0,size=0;
  char fromVersion[24]={},targetVersion[24]={},source[41]={};
  uint8_t fileHash[32]={},fromHash[32]={},targetHash[32]={};
  uint32_t checksum=0;
};
constexpr uint32_t RECORD_MAGIC=0x42505531U;
Record record;
bool haveRecord=false;
uint32_t recordChecksum(const Record& r){
  const uint8_t* p=reinterpret_cast<const uint8_t*>(&r);uint32_t hash=2166136261U;
  for(size_t i=0;i<offsetof(Record,checksum);i++){hash^=p[i];hash*=16777619U;}return hash;
}
bool validRecord(const Record& r){
  policy::Version from,to;
  return r.magic==RECORD_MAGIC&&r.schema==1&&r.stage>=uint32_t(Stage::Receiving)&&
    r.stage<=uint32_t(Stage::Interrupted)&&r.manual<=1&&r.checksum==recordChecksum(r)&&
    ((r.fromAddress==policy::SLOT0&&r.targetAddress==policy::SLOT1)||
     (r.fromAddress==policy::SLOT1&&r.targetAddress==policy::SLOT0))&&
    policy::terminated(r.fromVersion)&&policy::terminated(r.targetVersion)&&policy::terminated(r.source)&&
    policy::version(r.fromVersion,from)&&policy::version(r.targetVersion,to)&&
    (r.manual||(r.size>=policy::MIN_FIRMWARE_BYTES&&r.size<=policy::SLOT_BYTES&&policy::hex(r.source,40)));
}
bool readRecord(Record& out){
  Preferences prefs;if(!prefs.begin("bp-update",true))return false;
  Record candidate;
  const bool read=prefs.getBytesLength("record")==sizeof(candidate)&&
    prefs.getBytes("record",&candidate,sizeof(candidate))==sizeof(candidate);
  prefs.end();if(!read||!validRecord(candidate))return false;out=candidate;return true;
}
bool writeRecord(Record& next){
  next.magic=RECORD_MAGIC;next.schema=1;next.checksum=recordChecksum(next);
  if(!validRecord(next))return false;
  Preferences prefs;if(!prefs.begin("bp-update",false))return false;
  const bool written=prefs.putBytes("record",&next,sizeof(next))==sizeof(next);prefs.end();
  Record check;
  if(!written||!readRecord(check)||memcmp(&next,&check,sizeof(next)))return false;
  record=next;haveRecord=true;return true;
}
void recordStage(Stage stage){
  if(!haveRecord)return;
  Record next=record;next.stage=uint32_t(stage);writeRecord(next);
}
const esp_partition_t* partitionAt(uint32_t address){
  return slot0&&slot0->address==address?slot0:(slot1&&slot1->address==address?slot1:nullptr);
}
bool imageState(const esp_partition_t* p,esp_ota_img_states_t& result){
  result=ESP_OTA_IMG_UNDEFINED;return p&&esp_ota_get_state_partition(p,&result)==ESP_OK;
}
bool validImage(const esp_partition_t* p,uint32_t exactSize=0){
  if(!p)return false;
  esp_partition_pos_t position{};position.offset=p->address;position.size=p->size;
  esp_image_metadata_t image{};
  if(esp_image_verify(ESP_IMAGE_VERIFY,&position,&image)!=ESP_OK)return false;
  uint8_t prefix[36];
  return (!exactSize||image.image_len==exactSize)&&
    esp_partition_read(p,0,prefix,sizeof(prefix))==ESP_OK&&policy::validImagePrefix(prefix,sizeof(prefix));
}
bool layoutReady(){
  esp_chip_info_t chip{};esp_chip_info(&chip);
  uint32_t flashBytes=0;if(chip.model!=CHIP_ESP32P4||chip.revision!=policy::CHIP_REVISION||
     esp_flash_get_size(esp_flash_default_chip,&flashBytes)!=ESP_OK)return false;
  slot0=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,"app0");
  slot1=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,"app1");
  running=esp_ota_get_running_partition();
  policy::Hardware h{flashBytes,slot0?slot0->address:0,slot1?slot1->address:0,
    slot0?slot0->size:0,policy::CHIP_ID,uint16_t(chip.revision)};
  if(!policy::validHardware(h)||!slot1||slot1->size!=policy::SLOT_BYTES||!running||
     (running->address!=policy::SLOT0&&running->address!=policy::SLOT1))return false;
  uint8_t partitionTable[4096];
  if(esp_flash_read(esp_flash_default_chip,partitionTable,0x8000,sizeof(partitionTable))!=ESP_OK||
     !policy::validPartitionTable(partitionTable,sizeof(partitionTable)))return false;
  const esp_partition_t* data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,nullptr);
  return data&&data->address==0xE000&&data->size==0x2000;
}
bool bootloaderMatches(){
#if defined(BOILER_BOOTLOADER_SHA256) && defined(BOILER_BOOTLOADER_BYTES)
  const uint32_t size=BOILER_BOOTLOADER_BYTES;uint8_t expected[32],actual[32],buffer[1024];
  if(!size||size>0x6000||!policy::decodeHash(BOILER_BOOTLOADER_SHA256,expected))return false;
  mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);
  bool ok=mbedtls_sha256_starts(&sha,0)==0;
  for(uint32_t at=0;ok&&at<size;){
    const uint32_t n=size-at<sizeof(buffer)?size-at:sizeof(buffer);
    ok=esp_flash_read(esp_flash_default_chip,buffer,0x2000+at,n)==ESP_OK&&
       mbedtls_sha256_update(&sha,buffer,n)==0;at+=n;vTaskDelay(1);
  }
  if(ok)ok=mbedtls_sha256_finish(&sha,actual)==0&&policy::sameHash(actual,expected);
  mbedtls_sha256_free(&sha);return ok;
#else
  return false;
#endif
}
bool originalUsbState(){
  // Only the documented v0.9.1 USB seed is accepted. Unknown otadata is not repaired.
  if(!running||running->address!=policy::SLOT0)return false;
  const esp_partition_t* boot=esp_ota_get_boot_partition();
  if(!boot||boot->address!=running->address)return false;
  const esp_partition_t* data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,nullptr);
  uint8_t first[32],second[32];
  if(!data||esp_partition_read(data,0,first,sizeof(first))!=ESP_OK||
     esp_partition_read(data,0x1000,second,sizeof(second))!=ESP_OK)return false;
  if(policy::read32(first)!=1||policy::read32(first+24)!=uint32_t(ESP_OTA_IMG_UNDEFINED)||
     policy::read32(first+28)!=esp_rom_crc32_le(UINT32_MAX,first,4)||policy::read32(second)!=0)return false;
  for(unsigned i=4;i<24;i++)if(first[i]!=255)return false;
  for(unsigned i=4;i<32;i++)if(second[i]!=255)return false;
  uint8_t tail[256];
  for(unsigned sector=0;sector<2;sector++)for(uint32_t at=32;at<4096;){
    const uint32_t n=4096-at<sizeof(tail)?4096-at:sizeof(tail);
    if(esp_partition_read(data,sector*4096+at,tail,n)!=ESP_OK)return false;
    for(uint32_t i=0;i<n;i++)if(tail[i]!=255)return false;
    at+=n;
  }
  return true;
}
bool currentStateReady(bool normalize){
  if(!running)return false;
  const esp_partition_t* boot=esp_ota_get_boot_partition();
  if(!boot||boot->address!=running->address)return false;
  esp_ota_img_states_t status;
  const bool known=imageState(running,status);
  if(known&&status==ESP_OTA_IMG_VALID)return true;
  if((!known||status==ESP_OTA_IMG_UNDEFINED)&&originalUsbState()){
    if(!normalize)return true;
    if(!healthy(millis())||!validImage(running))return false;
    if(esp_ota_mark_app_valid_cancel_rollback()!=ESP_OK)return false;
    return imageState(running,status)&&status==ESP_OTA_IMG_VALID;
  }
  return false;
}
bool previousReady(){
  if(!haveRecord||record.stage!=uint32_t(Stage::Confirmed)||!running||
     running->address!=record.targetAddress||strcmp(panel::VERSION,record.targetVersion))return false;
  const esp_partition_t* previous=partitionAt(record.fromAddress);esp_ota_img_states_t status;
  uint8_t hash[32];
  return imageState(previous,status)&&status==ESP_OTA_IMG_VALID&&validImage(previous)&&
    esp_partition_get_sha256(previous,hash)==ESP_OK&&policy::sameHash(hash,record.fromHash);
}
void updateCapabilities(){
  const bool canRollback=platformReady&&!pendingBoot&&previousReady();
  esp_ota_img_states_t currentStatus;
  const bool nativeRollback=platformReady&&imageState(running,currentStatus)&&currentStatus==ESP_OTA_IMG_VALID&&currentStateReady(false);
  if(!lock())return;
  state.supported=platformReady;state.automaticRollback=nativeRollback;
  state.rollbackAvailable=canRollback;state.bootPending=pendingBoot;
  if(haveRecord)snprintf(state.previousVersion,sizeof(state.previousVersion),"%s",record.fromVersion);
  unlock();
}

void deadlineExpired(void*){
  if(bootConfirmed.load())return;
  const esp_partition_t* p=esp_ota_get_running_partition();esp_ota_img_states_t status;
  if(p&&esp_ota_get_state_partition(p,&status)==ESP_OK&&status==ESP_OTA_IMG_PENDING_VERIFY)esp_restart();
}
} // namespace

// Called before setup by Arduino's weak-hook replacement below. The timer task
// enforces the reset even if setup, the UI task or the update worker hangs.
void deferArduinoConfirmation(){
  const esp_partition_t* p=esp_ota_get_running_partition();esp_ota_img_states_t status;
  if(!p||esp_ota_get_state_partition(p,&status)!=ESP_OK||status!=ESP_OTA_IMG_PENDING_VERIFY)return;
  esp_timer_create_args_t args{};args.callback=deadlineExpired;args.name="ota-boot-guard";
  if(esp_timer_create(&args,&bootDeadline)!=ESP_OK||
     esp_timer_start_once(bootDeadline,uint64_t(policy::BOOT_DEADLINE_MS)*1000U)!=ESP_OK)esp_restart();
}

namespace {
void reconcileBoot(){
  haveRecord=readRecord(record);
  esp_ota_img_states_t current=ESP_OTA_IMG_UNDEFINED;
  const bool known=imageState(running,current);
  pendingBoot=known&&current==ESP_OTA_IMG_PENDING_VERIFY;
  if(pendingBoot){
    uint8_t hash[32];
    pendingIdentityOK=platformReady&&haveRecord&&record.stage==uint32_t(Stage::Armed)&&
      running->address==record.targetAddress&&!strcmp(panel::VERSION,record.targetVersion)&&
      esp_partition_get_sha256(running,hash)==ESP_OK&&policy::sameHash(hash,record.targetHash)&&
      esp_ota_check_rollback_is_possible();
    if(lock()){
      state.bootPending=true;state.busy=true;state.cancelAllowed=false;state.phase=Phase::Verifying;
      snprintf(state.message,sizeof(state.message),"%s",pendingIdentityOK?
        "Проверка запуска новой версии и интерфейса…":"Проверка запуска не пройдена. Будет возвращена прежняя версия.");
      state.errorCode=pendingIdentityOK?0:HealthError;unlock();
    }
    logEvent(pendingIdentityOK?"boot pending: checking UI":"boot pending: identity or fallback invalid");
    return;
  }
  if(!haveRecord)return;
  const esp_partition_t* target=partitionAt(record.targetAddress);esp_ota_img_states_t targetState;
  const bool aborted=imageState(target,targetState)&&(targetState==ESP_OTA_IMG_ABORTED||targetState==ESP_OTA_IMG_INVALID);
  uint8_t currentHash[32];
  const bool targetValid=known&&current==ESP_OTA_IMG_VALID&&running->address==record.targetAddress&&
    !strcmp(panel::VERSION,record.targetVersion)&&esp_partition_get_sha256(running,currentHash)==ESP_OK&&
    policy::sameHash(currentHash,record.targetHash);
  const bool recoveringConfirmation=record.stage==uint32_t(Stage::Armed)&&targetValid;
  const auto observation=policy::observeBoot(running?running->address:0,panel::VERSION,
    record.fromAddress,record.fromVersion,record.targetAddress,record.targetVersion,
    record.stage==uint32_t(Stage::Armed)||record.stage==uint32_t(Stage::Confirmed),
    targetValid&&(record.stage==uint32_t(Stage::Confirmed)||recoveringConfirmation),false,aborted);
  if(observation==policy::BootObservation::RolledBack){
    recordStage(Stage::RolledBack);
    char message[192];snprintf(message,sizeof(message),"Новая версия не прошла проверку. Возвращена %s.",record.fromVersion);
    finish(Phase::RolledBack,message,HealthError);logEvent("native rollback: previous app restored");
  }else if(observation==policy::BootObservation::Confirmed){
    if(recoveringConfirmation){
      Record recovered=record;recovered.stage=uint32_t(Stage::Confirmed);
      if(!writeRecord(recovered)){
        finish(Phase::Error,"Запуск уже проверен, но запись результата всё ещё недоступна.",PersistError);return;
      }
    }
    finish(record.manual?Phase::RolledBack:Phase::Success,
      record.manual?"Предыдущая версия восстановлена; запуск подтверждён.":"Обновление установлено; запуск подтверждён.");
  }else if(observation==policy::BootObservation::Interrupted||record.stage==uint32_t(Stage::Receiving)){
    recordStage(Stage::Interrupted);finish(Phase::Error,"Предыдущее обновление прервано. Рабочая версия сохранена.",Interrupted);
    logEvent("interrupted OTA: running app retained");
  }else if(observation==policy::BootObservation::Mismatch){
    recordStage(Stage::Interrupted);finish(Phase::Error,"Запись обновления не совпала с запущенной версией.",ImageError);
    logEvent("OTA boot record does not match app");
  }else if(record.stage==uint32_t(Stage::RolledBack)){
    finish(Phase::RolledBack,"После неудачного запуска восстановлена прежняя версия.",HealthError);
  }else if(record.stage==uint32_t(Stage::Interrupted)){
    finish(Phase::Error,"Предыдущее обновление было прервано. Можно проверить выпуск снова.",Interrupted);
  }
}
void checkBootHealth(){
  if(!pendingBoot||!pendingIdentityOK||!healthy(millis()))return;
  // Commit VALID first. If any preceding step fails, the independent timer stays armed.
  const esp_err_t valid=esp_ota_mark_app_valid_cancel_rollback();
  esp_ota_img_states_t status;
  if(valid!=ESP_OK||!imageState(running,status)||status!=ESP_OTA_IMG_VALID){
    pendingIdentityOK=false;phase(Phase::Error,"Не удалось подтвердить запуск. Будет выполнен откат.",valid?valid:HealthError);return;
  }
  bootConfirmed.store(true);
  if(bootDeadline)esp_timer_stop(bootDeadline);
  pendingBoot=false;
  Record confirmed=record;confirmed.stage=uint32_t(Stage::Confirmed);
  if(!writeRecord(confirmed)){
    finish(Phase::Error,"Запуск проверен, но результат не записан в память обновлений.",PersistError);
    logEvent("app VALID; confirmation record write failed");
  }else{
    finish(record.manual?Phase::RolledBack:Phase::Success,record.manual?
      "Предыдущая версия восстановлена. Интерфейс запущен.":"Обновление завершено. Интерфейс запущен.");
    logEvent(record.manual?"manual rollback confirmed":"new app confirmed after healthy UI");
  }
  updateCapabilities();
}

struct Headers {
  char location[policy::MAX_URL]={};
  uint32_t count=0,bytes=0;uint64_t length=0;
  bool bad=false,range=false,encoded=false,hasLength=false,chunked=false,hasLocation=false;
};
esp_err_t httpEvent(esp_http_client_event_t* event){
  if(!event||event->event_id!=HTTP_EVENT_ON_HEADER||!event->user_data)return ESP_OK;
  auto& h=*static_cast<Headers*>(event->user_data);
  const char* key=event->header_key;const char* value=event->header_value;
  if(!key||!value){h.bad=true;return ESP_OK;}
  const size_t keySize=strnlen(key,129),valueSize=strnlen(value,8193);
  h.bytes+=uint32_t(keySize+valueSize);if(++h.count>96||keySize>128||valueSize>8192||h.bytes>24576)h.bad=true;
  if(!strcasecmp(key,"Location")){
    if(h.hasLocation||valueSize>=sizeof(h.location))h.bad=true;
    else {memcpy(h.location,value,valueSize+1);h.hasLocation=true;}
  }else if(!strcasecmp(key,"Content-Range"))h.range=true;
  else if(!strcasecmp(key,"Content-Encoding")){if(strcasecmp(value,"identity"))h.encoded=true;}
  else if(!strcasecmp(key,"Transfer-Encoding")){
    if(strcasecmp(value,"chunked"))h.bad=true;else h.chunked=true;
  }else if(!strcasecmp(key,"Content-Length")){
    if(h.hasLength||!valueSize||valueSize>10){h.bad=true;return ESP_OK;}
    h.hasLength=true;h.length=0;
    for(size_t i=0;i<valueSize;i++){
      if(value[i]<'0'||value[i]>'9'){h.bad=true;break;}
      h.length=h.length*10+uint64_t(value[i]-'0');
    }
  }
  return ESP_OK;
}
struct HttpStream {
  esp_http_client_handle_t client=nullptr;Headers headers;
  int32_t error=Http;uint32_t started=0;
  ~HttpStream(){close();}
  void close(){if(client){esp_http_client_close(client);esp_http_client_cleanup(client);client=nullptr;}}
  bool open(const char* initial,policy::Resource kind,uint32_t maxSize,uint32_t exactSize=0){
    started=millis();char url[policy::MAX_URL];
    if(!policy::copy(url,initial)){error=Response;return false;}
    for(unsigned redirects=0;redirects<=4;redirects++){
      if(cancelRequested.load()){error=CancelledError;return false;}
      if(uint32_t(millis()-started)>30000){error=Timeout;return false;}
      if(!policy::allowedUrl(url,kind,initial)){error=Response;return false;}
      headers=Headers{};
      esp_http_client_config_t options{};options.url=url;options.method=HTTP_METHOD_GET;
      options.transport_type=HTTP_TRANSPORT_OVER_SSL;options.crt_bundle_attach=esp_crt_bundle_attach;
      options.timeout_ms=8000;options.buffer_size=4096;options.buffer_size_tx=4096;
      options.disable_auto_redirect=true;options.keep_alive_enable=false;
      options.user_agent="BoilerPanel-OTA/1";options.event_handler=httpEvent;options.user_data=&headers;
      client=esp_http_client_init(&options);
      if(!client){error=Memory;return false;}
      // No bearer token, cookies or saved credentials are sent, including redirects.
      esp_http_client_set_header(client,"Accept-Encoding","identity");
      esp_http_client_set_header(client,"Accept",kind==policy::Resource::Api?"application/vnd.github+json":"application/octet-stream");
      if(kind==policy::Resource::Api)esp_http_client_set_header(client,"X-GitHub-Api-Version","2022-11-28");
      const esp_err_t opened=esp_http_client_open(client,0);
      if(opened!=ESP_OK){error=opened;return false;}
      const int64_t length=esp_http_client_fetch_headers(client);
      const int status=esp_http_client_get_status_code(client);
      if(length<0||headers.bad){error=Response;return false;}
      if(status==301||status==302||status==303||status==307||status==308){
        if(redirects==4||!headers.hasLocation||!policy::copy(url,headers.location)){
          error=Response;return false;
        }
        close();continue;
      }
      if(status!=200){error=status;return false;}
      if(headers.range||headers.encoded||(headers.chunked&&headers.hasLength)||
         (headers.hasLength&&(headers.length>maxSize||(exactSize&&headers.length!=exactSize)))||
         uint64_t(length)>maxSize){error=Response;return false;}
      return true;
    }
    error=Response;return false;
  }
  int read(uint8_t* out,size_t size,uint32_t deadline=300000){
    if(cancelRequested.load()){error=CancelledError;return -1;}
    if(uint32_t(millis()-started)>deadline){error=Timeout;return -1;}
    const int n=esp_http_client_read(client,reinterpret_cast<char*>(out),int(size));
    if(n<0){error=Network;return -1;}
    if(!n&&!esp_http_client_is_complete_data_received(client)){error=Response;return -1;}
    if(headers.bad||headers.range||headers.encoded){error=Response;return -1;}
    return n;
  }
};

bool networkReady(){panel::RadioState radio;return panel_radio::snapshot(radio)&&radio.connected;}
bool clockReady(){const time_t now=time(nullptr);return now>=1704067200LL&&now<4102444800LL;}
bool prerequisites(){
  if(!platformReady){finish(Phase::Error,"Эта сборка или разметка Flash не поддерживает безопасное OTA.",Unsupported);return false;}
  if(!networkReady()){finish(Phase::Error,"Подключите панель к Wi-Fi для проверки GitHub.",Network);return false;}
  if(!clockReady()){finish(Phase::Error,"Ожидается точное время NTP. Повторите после синхронизации часов.",Clock);return false;}
  return true;
}
void transferFailure(int32_t code,bool writing){
  if(code==CancelledError||cancelRequested.load()){
    if(writing)recordStage(Stage::Cancelled);
    finish(Phase::Cancelled,writing?"Загрузка отменена. Рабочая прошивка сохранена.":"Проверка отменена.");logEvent("OTA cancelled; boot slot unchanged");return;
  }
  if(writing)recordStage(Stage::Failed);
  const char* message=code==HashError?"SHA-256 не совпал. Обновление отклонено; рабочая версия сохранена.":
    code==ManifestError?"Манифест выпуска не подходит этой панели или изменился.":
    code==ImageError?"Образ прошивки не прошёл проверку. Рабочая версия сохранена.":
    code==PersistError?"Не удалось сохранить состояние обновления во Flash.":
    code==Timeout?"GitHub отвечает слишком долго. Повторите обновление позже.":
    code==Response?"Ответ GitHub неполный или не соответствует размеру выпуска.":
    code==404?"Для этого выпуска не найден готовый OTA-пакет.":
    code==403||code==429?"GitHub временно ограничил запросы. Повторите позже.":
    "Не удалось загрузить выпуск по защищённому соединению. Проверьте Wi-Fi и время.";
  finish(Phase::Error,message,code);logEvent(writing?"OTA failed; boot slot unchanged":"release check failed");
}
bool getBody(const char* url,policy::Resource kind,size_t limit,uint32_t expected,
             std::unique_ptr<char[]>& body,size_t& received,int32_t& error){
  body.reset(new(std::nothrow) char[limit+1]);received=0;if(!body){error=Memory;return false;}
  HttpStream stream;if(!stream.open(url,kind,uint32_t(limit),expected)){error=stream.error;return false;}
  for(;;){
    const size_t remaining=limit+1-received;
    const int n=stream.read(reinterpret_cast<uint8_t*>(body.get()+received),remaining<4096?remaining:4096,30000);
    if(n<0){error=stream.error;return false;}
    if(!n)break;
    received+=size_t(n);
    if(received>limit){error=Response;return false;}
  }
  if(!received||(expected&&received!=expected)){error=Response;return false;}
  body[received]=0;return true;
}
void checkRelease(){
  if(!prerequisites())return;
  phase(Phase::Checking,"Проверка последнего стабильного выпуска на GitHub…");
  std::unique_ptr<char[]> body;size_t size=0;int32_t error=0;
  policy::Release found;
  if(!getBody(policy::LATEST_URL,policy::Resource::Api,policy::MAX_API_BYTES,0,body,size,error)){
    transferFailure(error,false);return;
  }
  if(!policy::parseRelease(body.get(),size,found)){transferFailure(ManifestError,false);return;}
  policy::Version foundVersion,currentVersion;
  if(!policy::version(found.version,foundVersion)||!policy::version(panel::VERSION,currentVersion)){
    transferFailure(VersionError,false);return;
  }
  const int comparison=policy::compare(foundVersion,currentVersion);
  if(comparison<0){
    haveOffer=false;
    if(lock()){
      state.available=false;state.lastCheck=uint32_t(time(nullptr));state.done=state.total=0;state.percent=0;
      snprintf(state.latestVersion,sizeof(state.latestVersion),"%s",found.version);unlock();
    }
    finish(Phase::Current,"Установлена актуальная стабильная версия.");logEvent("release check: current");return;
  }
  policy::Manifest candidate;
  if(!getBody(found.manifestUrl,policy::Resource::Asset,policy::MAX_MANIFEST_BYTES,found.manifestSize,body,size,error)){
    transferFailure(error,false);return;
  }
  if(!policy::parseManifest(body.get(),size,found.version,candidate)||candidate.size!=found.firmwareSize||
     strcmp(candidate.url,found.firmwareUrl)){transferFailure(ManifestError,false);return;}
  if(cancelRequested.load()){transferFailure(CancelledError,false);return;}
  if(comparison==0){
    haveOffer=false;
    if(lock()){
      state.available=false;state.lastCheck=uint32_t(time(nullptr));state.done=state.total=0;state.percent=0;
      snprintf(state.latestVersion,sizeof(state.latestVersion),"%s",found.version);unlock();
    }
    finish(Phase::Current,"Установлена актуальная версия. Манифест GitHub проверен.");logEvent("current release manifest verified");return;
  }
  offered=candidate;release=found;haveOffer=true;
  if(lock()){
    state.available=true;state.lastCheck=uint32_t(time(nullptr));state.done=state.total=0;state.percent=0;
    snprintf(state.latestVersion,sizeof(state.latestVersion),"%s",candidate.version);unlock();
  }
  finish(Phase::Available,"Доступен новый выпуск. Установка начнётся только после подтверждения.");
  logEvent("stable update available; awaiting user");
}

struct OtaWrite {
  esp_ota_handle_t handle=0;bool active=false;
  ~OtaWrite(){if(active)esp_ota_abort(handle);}
  bool begin(const esp_partition_t* target,uint32_t size,int32_t& error){
    const esp_err_t result=esp_ota_begin(target,size,&handle);error=result;active=result==ESP_OK;return active;
  }
  bool write(const uint8_t* data,size_t size,int32_t& error){
    const esp_err_t result=esp_ota_write(handle,data,size);error=result;return result==ESP_OK;
  }
  bool end(int32_t& error){
    const esp_err_t result=esp_ota_end(handle);active=false;error=result;return result==ESP_OK;
  }
};
struct Sha256 {
  mbedtls_sha256_context context;bool ready=false;
  Sha256(){mbedtls_sha256_init(&context);ready=mbedtls_sha256_starts(&context,0)==0;}
  ~Sha256(){mbedtls_sha256_free(&context);}
  bool add(const uint8_t* data,size_t n){return ready&&mbedtls_sha256_update(&context,data,n)==0;}
  bool finish(uint8_t (&hash)[32]){return ready&&mbedtls_sha256_finish(&context,hash)==0;}
};
void progress(uint32_t done,uint32_t total){
  if(!lock())return;
  state.done=done;state.total=total;
  state.percent=total?uint8_t((uint64_t(done)*100U)/total):0;unlock();
}
bool enterCommit(){
  if(!lock(100))return false;
  if(cancelRequested.load()){unlock();return false;}
  state.cancelAllowed=false;state.phase=Phase::Installing;
  snprintf(state.message,sizeof(state.message),"%s","Образ проверен. Подготовка безопасного запуска…");
  unlock();return true;
}
bool prepareInstall(bool network=true){
  if(network){if(!prerequisites())return false;}
  else if(!platformReady){finish(Phase::Error,"Локальный откат недоступен для этой разметки или загрузчика.",Unsupported);return false;}
  if(!healthy(millis())){finish(Phase::Error,"Дождитесь подтверждения запуска интерфейса.",HealthError);return false;}
  if(!layoutReady()||!bootloaderMatches()){
    platformReady=false;updateCapabilities();finish(Phase::Error,"Загрузчик или разметка Flash не совпали с проверенным комплектом.",BootloaderError);return false;
  }
  if(!currentStateReady(true)||!validImage(running)){
    finish(Phase::Error,"Текущая прошивка ещё не подтверждена или выбран другой слот запуска.",RunningState);return false;
  }
  return true;
}
void rebootTo(const esp_partition_t* target){
  // Caller has already persisted all recovery metadata and validated the image.
  const esp_err_t result=esp_ota_set_boot_partition(target);
  if(result!=ESP_OK){
    const esp_partition_t* selected=esp_ota_get_boot_partition();
    if(selected&&selected->address==running->address&&currentStateReady(false))recordStage(Stage::Failed);
    // Retain Armed when a failed write leaves selection uncertain; recovery must
    // still recognize the complete, authenticated target if it boots next time.
    finish(Phase::Error,"Не удалось выбрать новую прошивку для запуска.",result);return;
  }
  const esp_partition_t* boot=esp_ota_get_boot_partition();
  if(!boot||boot->address!=target->address){
    // Do not re-select the running partition: set_boot would turn its sole VALID
    // entry into NEW. Preserve that fallback and Armed metadata across a power
    // cut; the next real boot can recognize either the old or the target slot.
    finish(Phase::Error,"Не удалось подтвердить выбор загрузки. Текущая версия продолжает работу.",RunningState);return;
  }
  if(lock()){
    state.available=false;state.rollbackAvailable=false;state.phase=Phase::Rebooting;state.errorCode=0;
    snprintf(state.message,sizeof(state.message),"%s","Перезапуск. Новая версия подтвердится после запуска интерфейса.");unlock();
  }
  logEvent(record.manual?"manual rollback: reboot scheduled":"verified OTA: reboot scheduled");
  vTaskDelay(pdMS_TO_TICKS(900));esp_restart();
}
void installRelease(const char* expected){
  if(!haveOffer||strcmp(expected,offered.version)||!policy::newer(offered.version,panel::VERSION)){
    finish(Phase::Error,"Выпуск изменился. Проверьте обновления и подтвердите нужную версию.",VersionError);return;
  }
  if(!prepareInstall())return;
  phase(Phase::Checking,"Повторная проверка манифеста перед установкой…");
  std::unique_ptr<char[]> body;size_t size=0;int32_t error=0;policy::Manifest candidate;
  if(!getBody(release.manifestUrl,policy::Resource::Asset,policy::MAX_MANIFEST_BYTES,release.manifestSize,body,size,error)){
    transferFailure(error,false);return;
  }
  if(!policy::parseManifest(body.get(),size,expected,candidate)||candidate.size!=offered.size||
     strcmp(candidate.sha256,offered.sha256)||strcmp(candidate.source,offered.source)||strcmp(candidate.url,offered.url)){
    haveOffer=false;if(lock()){state.available=false;unlock();}transferFailure(ManifestError,false);return;
  }
  body.reset();
  const esp_partition_t* target=running->address==policy::SLOT0?slot1:slot0;
  const esp_partition_t* chosen=esp_ota_get_next_update_partition(nullptr);
  if(!target||!chosen||chosen->address!=target->address||target->address==running->address){
    finish(Phase::Error,"Не найден подходящий неактивный слот прошивки.",HardwareError);return;
  }
  HttpStream stream;
  if(!stream.open(candidate.url,policy::Resource::Asset,candidate.size,candidate.size)){
    transferFailure(stream.error,false);return;
  }
  uint8_t prefix[288];size_t prefixSize=0;
  while(prefixSize<sizeof(prefix)){
    const int n=stream.read(prefix+prefixSize,sizeof(prefix)-prefixSize);
    if(n<=0){transferFailure(n<0?stream.error:ImageError,false);return;}prefixSize+=size_t(n);
  }
  policy::TransferGate gate(candidate.size);gate.header(policy::validImagePrefix(prefix,prefixSize));
  if(!policy::validImagePrefix(prefix,prefixSize)){transferFailure(ImageError,false);return;}
  Record next;next.stage=uint32_t(Stage::Receiving);next.fromAddress=running->address;next.targetAddress=target->address;
  next.size=candidate.size;policy::copy(next.fromVersion,panel::VERSION);policy::copy(next.targetVersion,candidate.version);
  policy::copy(next.source,candidate.source);
  if(!policy::decodeHash(candidate.sha256,next.fileHash)||esp_partition_get_sha256(running,next.fromHash)!=ESP_OK){
    transferFailure(ImageError,false);return;
  }
  if(cancelRequested.load()){transferFailure(CancelledError,false);return;}
  if(!writeRecord(next)){transferFailure(PersistError,false);return;}
  if(lock()){state.rollbackAvailable=false;state.done=0;state.total=candidate.size;state.percent=0;unlock();}
  phase(Phase::Downloading,"Загрузка в свободный слот Flash. Рабочая версия сохраняется.");
  logEvent("OTA download started in inactive slot");
  OtaWrite writer;Sha256 hash;
  if(!hash.ready||!writer.begin(target,candidate.size,error)){transferFailure(error?error:HashError,true);return;}
  if(cancelRequested.load()){gate.cancel();transferFailure(CancelledError,true);return;}
  if(!gate.add(uint32_t(prefixSize))||!hash.add(prefix,prefixSize)||!writer.write(prefix,prefixSize,error)){
    gate.fail();transferFailure(error?error:ImageError,true);return;
  }
  progress(gate.received(),candidate.size);uint8_t buffer[4096];uint32_t lastProgress=millis();
  for(;;){
    const int n=stream.read(buffer,sizeof(buffer));
    if(n<0){gate.fail();transferFailure(stream.error,true);return;}
    if(!n)break;
    if(!gate.add(uint32_t(n))||!hash.add(buffer,size_t(n))||!writer.write(buffer,size_t(n),error)){
      gate.fail();transferFailure(error?error:ImageError,true);return;
    }
    if(uint32_t(millis()-lastProgress)>=100){progress(gate.received(),candidate.size);lastProgress=millis();}
    vTaskDelay(1);
  }
  stream.close();progress(gate.received(),candidate.size);
  phase(Phase::Verifying,"Проверка размера, SHA-256 и формата прошивки…");
  uint8_t actual[32];
  if(!hash.finish(actual)){gate.fail();transferFailure(HashError,true);return;}
  gate.digest(actual,next.fileHash);
  if(cancelRequested.load())gate.cancel();
  if(!gate.canEnd()){transferFailure(cancelRequested.load()?CancelledError:HashError,true);return;}
  const bool ended=writer.end(error);gate.end(ended);
  if(!ended||!validImage(target,candidate.size)||esp_partition_get_sha256(target,next.targetHash)!=ESP_OK){
    gate.fail();transferFailure(error?error:ImageError,true);return;
  }
  if(cancelRequested.load())gate.cancel();
  if(!gate.canSelect()||!enterCommit()){transferFailure(cancelRequested.load()?CancelledError:ImageError,true);return;}
  // Nothing after this point accepts cancellation. A partial/cancelled image can
  // never reach selection; a power cut leaves a durable from/target recovery map.
  next.stage=uint32_t(Stage::Armed);
  if(!writeRecord(next)){transferFailure(PersistError,true);return;}
  if(!currentStateReady(false)||!gate.canSelect()){
    recordStage(Stage::Failed);finish(Phase::Error,"Слот запуска изменился во время загрузки.",RunningState);return;
  }
  rebootTo(target);
}
void rollback(const char* expected){
  if(!prepareInstall(false))return;
  if(!previousReady()||strcmp(expected,record.fromVersion)){
    finish(Phase::Error,"Сохранённая предыдущая версия не совпала с подтверждением.",RollbackError);return;
  }
  const esp_partition_t* target=partitionAt(record.fromAddress);
  Record next;next.stage=uint32_t(Stage::Armed);next.manual=1;next.fromAddress=running->address;next.targetAddress=target->address;
  policy::copy(next.fromVersion,panel::VERSION);policy::copy(next.targetVersion,record.fromVersion);
  memcpy(next.targetHash,record.fromHash,sizeof(next.targetHash));
  if(esp_partition_get_sha256(running,next.fromHash)!=ESP_OK){finish(Phase::Error,"Текущий образ не прошёл проверку перед откатом.",ImageError);return;}
  if(!enterCommit()){transferFailure(CancelledError,false);return;}
  if(!writeRecord(next)){transferFailure(PersistError,false);return;}
  rebootTo(target);
}
bool startAutomaticCheck(){
  if(!lock())return false;
  if(state.busy||state.bootPending){unlock();return false;}
  cancelRequested.store(false);state.busy=true;state.cancelAllowed=true;state.phase=Phase::Checking;
  state.errorCode=0;unlock();return true;
}
void worker(void*){
  workerStartedAt=millis();
  platformReady=layoutReady()&&bootloaderMatches();
  reconcileBoot();
  if(!pendingBoot&&platformReady&&!currentStateReady(false))platformReady=false;
  updateCapabilities();
  if(!platformReady&&!pendingBoot)finish(Phase::Error,"Загрузчик, плата или разметка Flash не совпали с проверенным комплектом.",BootloaderError);
  for(;;){
    if(pendingBoot){checkBootHealth();vTaskDelay(pdMS_TO_TICKS(100));continue;}
    Request request;
    if(xQueueReceive(requests,&request,pdMS_TO_TICKS(100))==pdTRUE){
      if(request.action==Action::Check)checkRelease();
      else if(request.action==Action::Install)installRelease(request.version);
      else rollback(request.version);
      updateCapabilities();continue;
    }
    const uint32_t now=millis();
    if(platformReady&&networkReady()&&clockReady()&&uint32_t(now-workerStartedAt)>=30000&&
       (!automaticAttempted||uint32_t(now-lastAutomaticAttempt)>=policy::CHECK_INTERVAL_MS)&&startAutomaticCheck()){
      automaticAttempted=true;lastAutomaticAttempt=now;checkRelease();updateCapabilities();
    }
  }
}
#endif // BP_UPDATE_SUPPORTED

bool submit(Action action,const char* expected){
  if(!task||!requests||!lock())return false;
  if(!state.supported||state.busy||state.bootPending){unlock();return false;}
  Request request;request.action=action;
  if(action!=Action::Check){
    if(!expected||!policy::copy(request.version,expected)){unlock();return false;}
    const bool valid=action==Action::Install?
      state.available&&!strcmp(expected,state.latestVersion):
      state.rollbackAvailable&&!strcmp(expected,state.previousVersion);
    if(!valid){unlock();return false;}
  }
  cancelRequested.store(false);state.busy=true;state.cancelAllowed=action!=Action::Rollback;
  state.phase=Phase::Checking;state.errorCode=0;state.done=state.total=0;state.percent=0;
  snprintf(state.message,sizeof(state.message),"%s",action==Action::Check?"Проверка GitHub поставлена в очередь…":
    action==Action::Install?"Подготовка подтверждённого обновления…":"Проверка сохранённой предыдущей версии…");
  const bool queued=xQueueSend(requests,&request,0)==pdTRUE;
  if(!queued){state.busy=false;state.cancelAllowed=false;}
  unlock();return queued;
}
} // namespace

bool begin(){
  if(task)return true;
#if BP_UPDATE_SUPPORTED
  guard=xSemaphoreCreateMutex();requests=xQueueCreate(1,sizeof(Request));
  if(!guard||!requests){
    if(guard)vSemaphoreDelete(guard);
    if(requests)vQueueDelete(requests);
    guard=nullptr;requests=nullptr;return false;
  }
  state=Snapshot{};state.started=true;
  snprintf(state.message,sizeof(state.message),"%s","Проверка возможности безопасного обновления…");
  // parseRelease's bounded asset table and the TLS call chain need this stack.
  if(xTaskCreate(worker,"boiler-update",24576,nullptr,1,&task)!=pdPASS){
    task=nullptr;vSemaphoreDelete(guard);vQueueDelete(requests);guard=nullptr;requests=nullptr;return false;
  }
  return true;
#else
  return false;
#endif
}
bool snapshot(Snapshot& out){
  out=Snapshot{};if(!lock(10))return false;out=state;unlock();return true;
}
bool requestCheck(){return submit(Action::Check,nullptr);}
bool requestInstall(const char* expectedVersion){return submit(Action::Install,expectedVersion);}
bool requestRollback(const char* expectedPreviousVersion){return submit(Action::Rollback,expectedPreviousVersion);}
bool requestCancel(){
  if(!lock())return false;
  if(!state.busy||!state.cancelAllowed){unlock();return false;}
  cancelRequested.store(true);snprintf(state.message,sizeof(state.message),"%s","Отмена после завершения текущей операции…");unlock();return true;
}
void uiHeartbeat(bool ready){
  const uint32_t now=millis(),last=heartbeatAt.load();
  if(!ready){heartbeatReady.store(false);healthySince.store(now);heartbeatAt.store(now);return;}
  if(!heartbeatReady.load()||uint32_t(now-last)>policy::HEARTBEAT_MAX_GAP_MS)healthySince.store(now);
  heartbeatAt.store(now);heartbeatReady.store(true);
}
const char* phaseName(Phase p){
  switch(p){
    case Phase::Checking:return "checking";case Phase::Current:return "current";case Phase::Available:return "available";
    case Phase::Downloading:return "downloading";case Phase::Verifying:return "verifying";case Phase::Installing:return "installing";
    case Phase::Rebooting:return "rebooting";case Phase::Success:return "success";case Phase::Error:return "error";
    case Phase::Cancelled:return "cancelled";case Phase::RolledBack:return "rolled_back";default:return "idle";
  }
}
} // namespace panel_update

// Arduino 3.3.6 would otherwise mark PENDING_VERIFY valid before setup().
extern "C" bool verifyRollbackLater(void){
#if BP_UPDATE_SUPPORTED
  panel_update::deferArduinoConfirmation();
#endif
  return true;
}
