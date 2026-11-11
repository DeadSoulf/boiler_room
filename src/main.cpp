/* Boiler Panel v0.8.0 — calm physical menu, independent JC4880P443, LVGL 8.3.
 * Fuel level accepts normalized upper/lower reed readings; GPIO is not assigned.
 * No actuator outputs or borrowed heater logic.
 * Uses the existing, working portrait display/touch adapter and fonts.
 */
#include <Arduino.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "board_p4.h"
#include "lvgl_glue.h"
#include "panel_radio.h"
#include "panel_web.h"
#include "panel_ha.h"
#include "panel_storage.h"
#include "panel_update.h"
#include "panel_config.h"
#include "panel_sensors.h"
#include "panel_screen.h"
#include "panel_equipment.h"
#include "panel_boiler.h"
#include "panel_targets.h"
#include "panel_heating_runtime.h"
#include "panel_heating_settings.h"
#include <stdlib.h>
#if !LV_USE_BTNMATRIX || !LV_USE_TEXTAREA
#error "BoilerPanel requires LV_USE_BTNMATRIX=1 and LV_USE_TEXTAREA=1 in the existing lv_conf.h"
#endif

extern "C" {
extern const lv_font_t boiler_font_14;
extern const lv_font_t boiler_font_19;
extern const lv_font_t boiler_font_25;
extern const lv_font_t boiler_font_55;
}
namespace {
using namespace panel;
constexpr uint32_t BG=0x101719, CARD=0x1B2528, LINE=0x304044, TXT=0xEFF5F3;
constexpr uint32_t MUTED=0xA1B3B5, SKY=0x76D6C3, GREEN=0x76D6C3, AMBER=0xE7BB78, RED=0xEF9F9A;
const char* const PAGES[]={"Главная","Оборудование","Топливо","Настройки"};
constexpr int PAGE_SETTINGS=3;
enum MenuView { Root=0,WifiView=1,BleView=2,DisplayView=4,ConnectionsView=6,HaView=7,ServiceView=8,StorageView=9,DiagnosticsView=10,AboutView=11,WifiMoreView=12,UpdateView=13 };
int parentMenuView(int v){if(v==WifiMoreView)return WifiView;if(v==WifiView||v==BleView||v==HaView)return ConnectionsView;if(v==StorageView||v==DiagnosticsView||v==AboutView||v==UpdateView)return ServiceView;return Root;}
const char* const SENSOR_NAMES[]={"В доме","На улице","Подача","В баке","Обратка","На выходе"};
static_assert(sizeof(SENSOR_NAMES)/sizeof(SENSOR_NAMES[0])==CHANNELS,"Home labels must match telemetry channels");
Measurement measurements[CHANNELS]; // Populated only from CRC-verified real sensor events.
FuelLevel fuelLevel; // Populated from configured, debounced GPIO contacts only.
lv_obj_t *boilerOnLabel=nullptr,*boilerOffLabel=nullptr;
struct HeaterOutput {
  const char* name;
  panel_config::Channel channel;
};
constexpr HeaterOutput ELECTRIC_HEATERS[]={
  {"ТЭН-1",panel_config::Channel::ElectricRelay},
  {"ТЭН-2",panel_config::Channel::ElectricRelay2},
  {"ТЭН-3",panel_config::Channel::ElectricRelay3}
};
constexpr unsigned HEATER_COUNT=sizeof(ELECTRIC_HEATERS)/sizeof(ELECTRIC_HEATERS[0]);
lv_obj_t* heaterPinLabels[HEATER_COUNT]={};
lv_obj_t *autoValues[8]={},*waterReading=nullptr,*dhwRequests=nullptr,*electricValues[3]={};
lv_obj_t *roomFields[3]={},*roomGoal=nullptr,*roomGoalLabel=nullptr,*roomNowLabel=nullptr;
bool adjustingRooms=false;
lv_obj_t *loopRooms[8]={},*loopRequests[8]={};
panel_heating_settings::State roomDraft{};

lv_obj_t *boilerOnField=nullptr,*boilerOffField=nullptr,*boilerSaveCaption=nullptr;
panel_boiler::Target boilerDraftTarget=panel_boiler::Target::Dhw;
uint32_t boilerDraftRevision=0;
bool boilerConfirm=false;
lv_obj_t *haStatusLabel=nullptr;
struct TemperatureEditor {
  int channel=-1;
  panel_targets::Setting original{};
  bool edited=false;
  const char* saveError=nullptr;
  lv_obj_t *reading=nullptr,*selection=nullptr,*slider=nullptr,*decrease=nullptr,*increase=nullptr,*save=nullptr;
};
TemperatureEditor targetEditor;
struct FuelCanister {
  lv_obj_t *root=nullptr,*body=nullptr,*fill=nullptr,*handle=nullptr,*cap=nullptr;
  lv_obj_t *crossA=nullptr,*crossB=nullptr,*glyph=nullptr,*status=nullptr;
};
FuelCanister fuelCanister;
lv_obj_t* tempIcon[2]={};
constexpr int CANISTER_FILL_HEIGHT=24;
void updateCanister();
struct FuelLevelLabels {
  lv_obj_t *summary=nullptr,*upper=nullptr,*lower=nullptr,*upperDot=nullptr,*lowerDot=nullptr;
};
FuelLevelLabels fuelCardLabels,fuelDetailLabels;
lv_obj_t *fuelDetailHint=nullptr;
RadioState radio;
int page=0, pendingPage=-1, slideDirection=0, view=0;
uint32_t clicks=0, lastGesture=0, lastTick=0, renderedRevision=0, renderedSignature=0;
bool rebuild=false, closeDialog=false;
uint32_t lastSensorConfigRevision=0xffffffffU;
lv_obj_t *tempValue[CHANNELS]={},*tempState[CHANNELS]={},*tempTrend[CHANNELS]={},*freshBar[CHANNELS]={};
lv_obj_t *sensorSummary=nullptr,*sensorHint=nullptr,*liveTime=nullptr,*activity=nullptr,*activityDot=nullptr;
lv_obj_t *resultList=nullptr,*heapLabel=nullptr,*uptimeLabel=nullptr;
lv_obj_t *sdStatusLabel=nullptr,*sdFormatNotice=nullptr,*sdFormatAccept=nullptr,*sdFormatReject=nullptr;
panel_update::Snapshot updateState;
lv_obj_t *updateLatest=nullptr,*updateStatus=nullptr,*updateMessage=nullptr,*updateProgress=nullptr,*updateProgressFill=nullptr,*updateError=nullptr;
lv_obj_t *updateCheckButton=nullptr,*updateInstallButton=nullptr,*updateCancelButton=nullptr,*updateRollbackButton=nullptr;
lv_obj_t *homeUpdateNotice=nullptr,*homeUpdateText=nullptr;
char confirmedUpdateVersion[24]={};
bool confirmUpdateRollback=false,firstFrameShown=false,startupServicesReady=false;
int renderedPage=-1,renderedView=-1;
uint32_t activityStart=0;
bool wifiLogSeen=false,wifiLogWasOnline=false;
AccessPoint displayedAps[MAX_AP];uint8_t displayedApCount=0;
SavedNetwork displayedNetworks[MAX_WIFI_PROFILES],confirmedForget;
uint8_t displayedNetworkCount=0;
bool displaySettingsLoaded=false;
uint16_t appliedDisplayTimeout=0;
void info(const char* title,const char* description);
lv_obj_t *header=nullptr,*body=nullptr,*clockLabel=nullptr,*dateLabel=nullptr,*footer=nullptr;
lv_obj_t *headerTitle=nullptr,*pairBadge=nullptr,*pairDigits=nullptr;
uint32_t shownPairingGeneration=0;
lv_obj_t *overlay=nullptr,*dialogError=nullptr,*ssidField=nullptr,*passwordField=nullptr,*timeField=nullptr,*activeField=nullptr,*keyboard=nullptr;
int dialogKind=0; // 0 none, 1 Wi-Fi, 2 clock, 3 information, 4 fuel, 5 update, 6 boiler settings.
bool openNetwork=false, upper=false, symbols=false;
char selectedSsid[33]={};
bool ssidLocked=false;

lv_color_t c(uint32_t v){return lv_color_hex(v);}
// Unsupported glyphs in broadcast device names become '?', not squares.
// The raw Wi-Fi SSID used for connection is NEVER altered by this display filter.
void supportedText(const char* src,char* dst,size_t capacity,const lv_font_t* font){
  size_t p=0;const unsigned char* s=reinterpret_cast<const unsigned char*>(src);
  while(*s && p+1<capacity){
    uint32_t cp;unsigned n;
    if(*s<0x80){cp=*s;n=1;}
    else if((*s&0xE0)==0xC0){cp=*s&31;n=2;}
    else if((*s&0xF0)==0xE0){cp=*s&15;n=3;}
    else if((*s&0xF8)==0xF0){cp=*s&7;n=4;}
    else{dst[p++]='?';++s;continue;}
    bool valid=true;
    for(unsigned k=1;k<n;k++){if(s[k]==0 || (s[k]&0xC0)!=0x80){valid=false;break;}cp=(cp<<6)|(s[k]&63);}
    lv_font_glyph_dsc_t glyph={};
    if(valid && (cp=='\n' || lv_font_get_glyph_dsc(font,&glyph,cp,0)) && p+n<capacity){memcpy(dst+p,s,n);p+=n;}
    else dst[p++]='?';
    s+=valid?n:1;
  }
  dst[p]=0;
}
lv_obj_t* label(lv_obj_t* parent,const char* value,int x,int y,int width,const lv_font_t* font=&boiler_font_14,uint32_t ink=TXT){
  auto* o=lv_label_create(parent);char safe[384];supportedText(value,safe,sizeof(safe),font);lv_label_set_text(o,safe);
  lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,c(ink),0);lv_obj_set_style_text_line_space(o,3,0);
  lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);return o;
}
lv_obj_t* box(lv_obj_t* parent,int x,int y,int w,int h,uint32_t fill=CARD,int radius=18){
  auto* o=lv_obj_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_bg_color(o,c(fill),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
  lv_obj_set_style_border_color(o,c(LINE),0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_radius(o,radius,0);
  lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);return o;
}
enum Action:uintptr_t{Home=1,Back,Wifi,Ble,Time,WifiScan,WifiManual,WifiDisconnect,BleScan=10,ManualClock,ClockSave,Connect,Cancel,ToggleOpen,TogglePassword,ServiceRefresh,Zone0,Zone1,Zone2,Display,Timeout0=30,Timeout1,Timeout2,Timeout3,Timeout4,Timeout5,Timeout6,Temp0=40,FuelOverview=61,EquipmentDhw=70,EquipmentElectric,EquipmentDiesel,EquipmentFuel,Ap0=100,SdFormatApprove=120,SdFormatCancel,Connections=140,HaStatus,ServiceMenu,StorageMenu,DiagnosticsMenu,AboutMenu,WifiMore,ConfirmDisconnect,ConfirmForget,UpdateMenu,UpdateCheck,UpdateInstall,UpdateCancel,UpdateRollback,UpdateConfirm,SavedEdit0=170,SavedForget0=SavedEdit0+MAX_WIFI_PROFILES,BoilerEdit=200,BoilerSave,TargetLess,TargetMore,TargetSave,HeatingPage,RoomLimits,RoomSave,RoomCooler,RoomWarmer,ViewFloors,ViewRadiators};
void action(lv_event_t* e);
void updateEquipmentReadings();
void updateStorageLabel();
void updateUpdateLabels();
void confirmUpdate(bool rollback);
void confirmNetwork(const SavedNetwork* target=nullptr);
lv_obj_t* scrollList(int y,int height);
lv_obj_t* button(lv_obj_t* parent,const char* title,int x,int y,int w,int h,uintptr_t code,uint32_t fill=0x26373B,bool enabled=true){
  auto* o=lv_btn_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  lv_obj_set_style_bg_color(o,c(fill),0);lv_obj_set_style_bg_color(o,c(0x365A73),LV_STATE_PRESSED);
  lv_obj_set_style_radius(o,12,0);lv_obj_set_style_shadow_width(o,0,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_pad_all(o,3,0);
  lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(o,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(code));
  auto* t=label(o,title,0,0,w-12,&boiler_font_19);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(t);
  if(!enabled){lv_obj_add_state(o,LV_STATE_DISABLED);lv_obj_set_style_opa(o,LV_OPA_40,0);}return o;
}
void clearSecretFields(){
  if(passwordField)lv_textarea_set_text(passwordField,"");
  ssidField=passwordField=timeField=activeField=keyboard=dialogError=nullptr;
  memset(selectedSsid,0,sizeof(selectedSsid));
}
void dismiss(){
  boilerOnField=boilerOffField=boilerSaveCaption=nullptr;boilerConfirm=false;
  targetEditor=TemperatureEditor{};fuelDetailLabels={};
  for(auto& f:roomFields)f=nullptr;
  roomGoal=roomGoalLabel=roomNowLabel=nullptr;adjustingRooms=false;
  fuelDetailHint=nullptr;
  if(overlay){clearSecretFields();lv_obj_del(overlay);}overlay=nullptr;dialogKind=0;closeDialog=false;
  confirmedForget={};memset(confirmedUpdateVersion,0,sizeof(confirmedUpdateVersion));
}
void error(const char* msg){if(dialogError)lv_label_set_text(dialogError,msg);}
bool send(Command& command){
  panel_update::Snapshot update;const bool known=panel_update::snapshot(update);
  const bool updating=update.busy||update.bootPending;
  const bool ok=known&&!updating&&panel_radio::submit(command);
  // Erase the command copy after the queue has copied it.
  volatile char* p=command.password;for(size_t i=0;i<sizeof(command.password);i++)p[i]=0;
  if(!ok){
    const char* why=!known?"Не удалось проверить состояние обновления. Повторите попытку.":
      updating?"Дождитесь завершения обновления панели.":"Радио не готово или очередь занята. Повторите позже.";
    if(overlay)error(why);else info("Операция не принята",why);
  }
  else activityStart=millis();
  return ok;
}
void info(const char* title,const char* description){
  dismiss();dialogKind=3;overlay=box(lv_layer_top(),0,0,480,800,BG,0);lv_obj_set_style_bg_opa(overlay,LV_OPA_90,0);
  auto* p=box(overlay,18,185,444,390);label(p,title,20,22,402,&boiler_font_25);
  label(p,description,20,80,402,&boiler_font_19,MUTED);button(p,"Закрыть",20,318,402,52,Cancel);
}
void fieldFocus(lv_event_t* e){activeField=lv_event_get_target(e);}
const char* LOWER[]={"q","w","e","r","t","y","u","i","o","p","\n","a","s","d","f","g","h","j","k","l","\n","z","x","c","v","b","n","m","-","_","\n","ABC","123","Пробел","<-",""};
const char* UPPER[]={"Q","W","E","R","T","Y","U","I","O","P","\n","A","S","D","F","G","H","J","K","L","\n","Z","X","C","V","B","N","M","-","_","\n","abc","123","Пробел","<-",""};
const char* SYMBOLS[]={"1","2","3","4","5","6","7","8","9","0","\n","!","@","#","$","%","^","&","*","(",")","\n","-","_","+","=","[","]","{","}","\\","|","\n",".",",",":",";","/","?","'","\"","<",">","\n","abc","~","`","Пробел","<-",""};
void setKeyboardMap(const char* map[]){
  lv_btnmatrix_set_map(keyboard,map);
  // Change layouts only after release. A held finger must not hit the new map.
  // Reapply controls because maps with a different key count allocate new bits.
  for(uint16_t id=0;const char* text=lv_btnmatrix_get_btn_text(keyboard,id);++id){
    if(!strcmp(text,"ABC") || !strcmp(text,"abc") || !strcmp(text,"123"))
      lv_btnmatrix_set_btn_ctrl(keyboard,id,lv_btnmatrix_ctrl_t(LV_BTNMATRIX_CTRL_CLICK_TRIG|LV_BTNMATRIX_CTRL_NO_REPEAT));
  }
}
void keyboardEvent(lv_event_t* e){
  if(!activeField || lv_obj_has_state(activeField,LV_STATE_DISABLED))return;
  const uint16_t id=lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
  if(id==LV_BTNMATRIX_BTN_NONE)return;
  const char* t=lv_btnmatrix_get_btn_text(lv_event_get_target(e),id);if(!t)return;
  if(!strcmp(t,"ABC")){upper=true;symbols=false;setKeyboardMap(UPPER);}
  else if(!strcmp(t,"abc")){upper=false;symbols=false;setKeyboardMap(LOWER);}
  else if(!strcmp(t,"123")){symbols=true;setKeyboardMap(SYMBOLS);}
  else if(!strcmp(t,"<-"))lv_textarea_del_char(activeField);
  else if(!strcmp(t,"Пробел"))lv_textarea_add_text(activeField," ");
  else lv_textarea_add_text(activeField,t);
}
lv_obj_t* field(lv_obj_t* parent,const char* placeholder,int x,int y,int w,int length,bool secret=false){
  auto* t=lv_textarea_create(parent);lv_obj_set_pos(t,x,y);lv_obj_set_size(t,w,54);
  lv_textarea_set_one_line(t,true);lv_textarea_set_max_length(t,length);lv_textarea_set_placeholder_text(t,placeholder);lv_textarea_set_text(t,"");
  lv_obj_set_style_text_font(t,&boiler_font_19,0);lv_obj_set_style_text_color(t,c(TXT),0);
  lv_obj_set_style_bg_color(t,c(0x0D1B2B),0);lv_obj_set_style_border_color(t,c(LINE),0);lv_obj_set_style_radius(t,10,0);
  lv_textarea_set_password_mode(t,secret);lv_obj_clear_flag(t,LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(t,fieldFocus,LV_EVENT_CLICKED,nullptr);return t;
}
void makeKeyboard(int y){
  keyboard=lv_btnmatrix_create(overlay);lv_obj_set_pos(keyboard,12,y);lv_obj_set_size(keyboard,456,800-y-14);
  lv_obj_set_style_bg_color(keyboard,c(BG),0);lv_obj_set_style_border_width(keyboard,0,0);lv_obj_set_style_pad_all(keyboard,4,0);
  lv_obj_set_style_pad_row(keyboard,6,0);lv_obj_set_style_pad_column(keyboard,4,0);
  lv_obj_set_style_text_font(keyboard,&boiler_font_19,LV_PART_ITEMS);
  lv_obj_set_style_text_color(keyboard,c(TXT),LV_PART_ITEMS);lv_obj_set_style_bg_color(keyboard,c(0x20384E),LV_PART_ITEMS);
  lv_obj_set_style_bg_color(keyboard,c(0x3B647E),LV_PART_ITEMS|LV_STATE_PRESSED);
  lv_obj_set_style_border_width(keyboard,0,LV_PART_ITEMS);lv_obj_set_style_radius(keyboard,6,LV_PART_ITEMS);
  setKeyboardMap(LOWER);upper=false;symbols=false;
  lv_obj_clear_flag(keyboard,LV_OBJ_FLAG_GESTURE_BUBBLE);lv_obj_add_event_cb(keyboard,keyboardEvent,LV_EVENT_VALUE_CHANGED,nullptr);
}
void boilerSettingsChanged(lv_event_t*){
  if(!boilerConfirm)return;
  boilerConfirm=false;lv_label_set_text(boilerSaveCaption,"Сохранить");
  error("Проверьте оба порога и сохраните их вместе.");
}
panel_boiler::Target equipmentBoilerTarget(){
  return view==2?panel_boiler::Target::Electric:panel_boiler::Target::Dhw;
}
void editBoiler(){
  const auto target=equipmentBoilerTarget();
  panel_boiler::Settings current;
  if(!panel_boiler::snapshot(current,target)){info("Пороги недоступны","Не удалось прочитать настройки. Повторите позже.");return;}
  dismiss();dialogKind=6;boilerDraftTarget=target;boilerDraftRevision=current.revision;
  overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  label(overlay,target==panel_boiler::Target::Electric?"Пороги электрокотла":"Пороги бойлера",18,24,435,&boiler_font_25);
  label(overlay,"Включение при охлаждении, °C",18,92,444,&boiler_font_19);
  boilerOnField=field(overlay,"Не задано",18,133,444,2);
  label(overlay,"Выключение при нагреве, °C",18,210,444,&boiler_font_19);
  boilerOffField=field(overlay,"Не задано",18,251,444,2);
  for(auto* f:{boilerOnField,boilerOffField})lv_textarea_set_accepted_chars(f,"0123456789");
  if(current.configured){
    char value[6];snprintf(value,sizeof(value),"%u",unsigned(current.onC));lv_textarea_set_text(boilerOnField,value);
    snprintf(value,sizeof(value),"%u",unsigned(current.offC));lv_textarea_set_text(boilerOffField,value);
  }
  dialogError=label(overlay,"Введите целые градусы. Порог включения должен быть ниже порога выключения.",18,321,444,&boiler_font_14,MUTED);
  button(overlay,"Назад",18,390,144,56,Cancel);
  auto* save=button(overlay,"Сохранить",174,390,288,56,BoilerSave,0x245D55);
  boilerSaveCaption=lv_obj_get_child(save,0);
  for(auto* f:{boilerOnField,boilerOffField})lv_obj_add_event_cb(f,boilerSettingsChanged,LV_EVENT_VALUE_CHANGED,nullptr);
  static const char* digits[]={"1","2","3","\n","4","5","6","\n","7","8","9","\n","0","<-",""};
  makeKeyboard(468);setKeyboardMap(digits);activeField=boilerOnField;
}
int boilerDegrees(lv_obj_t* fieldObj){
  const char* p=lv_textarea_get_text(fieldObj);const size_t n=strlen(p);if(!n||n>2)return -1;
  int value=0;for(size_t i=0;i<n;++i){if(p[i]<'0'||p[i]>'9')return -1;value=value*10+p[i]-'0';}return value;
}
void saveBoiler(){
  if(!boilerOnField||!boilerOffField)return;
  int on=boilerDegrees(boilerOnField),off=boilerDegrees(boilerOffField);
  if(on<0||off<0){error("Введите обе температуры целыми числами от 0 до 99.");return;}
  if(on>=off){error("Температура выключения должна быть выше температуры включения.");return;}
  panel_update::Snapshot ota;
  if(!panel_update::snapshot(ota)||!ota.started||ota.busy||ota.bootPending){error("Сохранение недоступно. Проверьте состояние обновления.");return;}
  if(!boilerConfirm){
    char review[160];snprintf(review,sizeof(review),"Включение: %d °C. Выключение: %d °C. Подтвердите сохранение.",on,off);
    error(review);boilerConfirm=true;lv_label_set_text(boilerSaveCaption,"Подтвердить");return;
  }
  const auto r=panel_boiler::save(boilerDraftRevision,on,off,boilerDraftTarget);
  if(r==panel_boiler::Result::Saved){
    closeDialog=true;rebuild=true;return;
  }
  if(r==panel_boiler::Result::Conflict)error("Настройки уже изменились. Закройте и откройте редактор снова.");
  else if(r==panel_boiler::Result::Invalid)error("Проверьте значения обеих температур.");
  else error("Не удалось подтвердить запись. Проверьте настройки и повторите.");
}
bool readRoomFields(float (&degrees)[3],bool report=true);
void moveRoomLimits(float change,bool recenter=true);
void updateRoomGoal(bool recenter){
  if(!roomGoal || adjustingRooms)return;
  float t[3];if(!readRoomFields(t,false))return;
  if(recenter){
    // A twenty-degree touch range keeps tenths selectable on the 480px screen.
    // Direct entry still covers the full sensor range.
    const float lowest=fminf(t[0],fminf(t[1],t[2])),highest=fmaxf(t[0],fmaxf(t[1],t[2]));
    const int low=int(lroundf((t[1]+fmaxf(-10.0f,-55.0f-lowest))*10));
    const int high=int(lroundf((t[1]+fminf(10.0f,125.0f-highest))*10));
    if(low>=high)lv_obj_add_state(roomGoal,LV_STATE_DISABLED);
    else{lv_obj_clear_state(roomGoal,LV_STATE_DISABLED);lv_slider_set_range(roomGoal,low,high);}
  }
  adjustingRooms=true;
  lv_slider_set_value(roomGoal,int(lroundf(t[1]*10)),LV_ANIM_OFF);
  adjustingRooms=false;
  char caption[96];snprintf(caption,sizeof(caption),"Желаемая температура: %.1f °C",double(t[1]));
  lv_label_set_text(roomGoalLabel,caption);
}
void roomSliderChanged(lv_event_t*){
  if(adjustingRooms || !roomGoal)return;
  float t[3];if(!readRoomFields(t))return;
  moveRoomLimits(lv_slider_get_value(roomGoal)/10.f-t[1],false);
}
void roomFieldChanged(lv_event_t*){updateRoomGoal(true);}
void updateRoomObservation(){
  if(!roomNowLabel)return;
  const auto& measured=measurements[0];char caption[80];
  if(measured.fresh(millis()))snprintf(caption,sizeof(caption),"В доме: %.1f °C",double(measured.value));
  else snprintf(caption,sizeof(caption),"%s",measured.count?"В доме: устарело":"В доме: нет данных");
  if(strcmp(lv_label_get_text(roomNowLabel),caption))lv_label_set_text(roomNowLabel,caption);
}
void editRoomLimits(){
  panel_heating_settings::State saved{};
  if(!panel_heating_settings::snapshot(saved)){
    info("Пороги недоступны","Не удалось прочитать настройки отопления.");return;
  }
  dismiss();roomDraft=saved;
  overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  if(page==0){
    roomNowLabel=label(overlay,"",18,20,210,&boiler_font_19);
    button(overlay,"Справочная уставка",236,7,226,47,Temp0);
    updateRoomObservation();
  }else label(overlay,"Пороги комнат",18,22,430,&boiler_font_25);
  const char* captions[]={"Открыть контуры, °C","Закрыть батарею*, °C","Закрыть все контуры, °C"};
  const float values[]={saved.config.roomOnC,saved.config.roomRadiatorOffC,saved.config.roomOffC};
  for(unsigned i=0;i<3;i++){
    const int y=62+int(i)*60;
    label(overlay,captions[i],18,y+15,278,&boiler_font_19);
    auto* f=field(overlay,"°C",302,y,160,7);roomFields[i]=f;
    lv_textarea_set_accepted_chars(f,"-0123456789.,");
    char value[16];snprintf(value,sizeof(value),"%.1f",double(values[i]));
    lv_textarea_set_text(f,value);
  }
  label(overlay,"* В комнате с полом и батареей.",18,242,444,&boiler_font_14,MUTED);
  roomGoalLabel=label(overlay,"",18,275,444,&boiler_font_19,SKY);
  button(overlay,"-1 °C",18,314,72,52,RoomCooler);
  button(overlay,"+1 °C",390,314,72,52,RoomWarmer);
  roomGoal=lv_slider_create(overlay);
  lv_obj_set_pos(roomGoal,115,335);lv_obj_set_size(roomGoal,250,16);
  lv_obj_set_ext_click_area(roomGoal,18);
  lv_obj_set_style_bg_color(roomGoal,c(LINE),LV_PART_MAIN);
  lv_obj_set_style_bg_color(roomGoal,c(SKY),LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(roomGoal,c(TXT),LV_PART_KNOB);
  lv_obj_set_style_pad_all(roomGoal,9,LV_PART_KNOB);
  lv_obj_clear_flag(roomGoal,LV_OBJ_FLAG_GESTURE_BUBBLE);
  updateRoomGoal(true);
  lv_obj_add_event_cb(roomGoal,roomSliderChanged,LV_EVENT_VALUE_CHANGED,nullptr);
  for(auto* f:roomFields)lv_obj_add_event_cb(f,roomFieldChanged,LV_EVENT_VALUE_CHANGED,nullptr);
  dialogError=label(overlay,"Порог открытия < цель < закрытие всех.",18,379,444,&boiler_font_14,MUTED);
  button(overlay,"Назад",18,427,144,56,Cancel);
  button(overlay,"Сохранить",174,427,288,56,RoomSave,0x245D55);
  static const char* keys[]={"1","2","3","\n","4","5","6","\n","7","8","9","\n","-","0",",","<-",""};
  makeKeyboard(499);setKeyboardMap(keys);activeField=roomFields[0];
}
bool readRoomFields(float (&degrees)[3],bool report){
  for(unsigned i=0;i<3;i++){
    char text[12];snprintf(text,sizeof(text),"%s",lv_textarea_get_text(roomFields[i]));
    if(auto* comma=strchr(text,','))*comma='.';
    char* end=nullptr;degrees[i]=strtof(text,&end);
    if(end==text || *end || !isfinite(degrees[i]) || degrees[i]<-55 || degrees[i]>125){
      if(report)error("Введите три температуры от -55 до 125 °C.");
      return false;
    }
  }
  return true;
}
void moveRoomLimits(float change,bool recenter){
  float degrees[3];
  if(!roomFields[0]||!readRoomFields(degrees))return;
  for(const float value:degrees){
    if(value+change < -55.001f || value+change > 125.001f){error("Диапазон датчика: от -55 до 125 °C.");return;}
  }
  adjustingRooms=true;
  for(unsigned i=0;i<3;i++){
    char text[16];snprintf(text,sizeof(text),"%.1f",double(degrees[i]+change));
    lv_textarea_set_text(roomFields[i],text);
  }
  adjustingRooms=false;
  updateRoomGoal(recenter);
  error("Сохраните выбранные пороги.");
}
void saveRoomLimits(){
  if(!roomFields[0])return;
  panel_update::Snapshot ota;
  if(!panel_update::snapshot(ota)||!ota.started||ota.busy||ota.bootPending){error("Дождитесь готовности панели.");return;}
  float degrees[3];if(!readRoomFields(degrees))return;
  auto& cfg=roomDraft.config;
  cfg.roomOnC=degrees[0];cfg.roomRadiatorOffC=degrees[1];cfg.roomOffC=degrees[2];
  using Result=panel_heating_settings::Result;
  const auto r=panel_heating_settings::save(roomDraft.revision,roomDraft);
  if(r==Result::Saved){closeDialog=true;return;}
  if(r==Result::Conflict)error("Настройки изменились. Откройте пороги снова.");
  else if(r==Result::Invalid)error("Проверьте порядок порогов и шаг 0,1 °C.");
  else error("Сохранение не подтверждено. Повторите позже.");
}
void credentialsFor(const char* ssid,bool isOpen){
  dismiss();dialogKind=1;ssidLocked=ssid!=nullptr;openNetwork=isOpen;
  overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  label(overlay,"Подключение Wi-Fi",18,23,300,&boiler_font_25);button(overlay,"Домой",350,14,112,50,Home);
  if(ssid){
    snprintf(selectedSsid,sizeof(selectedSsid),"%s",ssid);
    // Sanitize only the caption; selectedSsid retains the bytes used to connect.
    char caption[sizeof(selectedSsid)];snprintf(caption,sizeof(caption),"%s",selectedSsid);
    for(char* p=caption;*p;++p)if(static_cast<unsigned char>(*p)<0x20 || *p==0x7F)*p=' ';
    label(overlay,"Выбранная сеть",18,88,442,&boiler_font_14,MUTED);
    auto* name=label(overlay,caption,18,116,442,&boiler_font_19);
    lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,52);
  }else{
    label(overlay,"Имя сети (SSID)",18,84,442,&boiler_font_14,MUTED);ssidField=field(overlay,"Введите SSID",18,108,444,32);
  }
  label(overlay,"Пароль",18,183,442,&boiler_font_14,MUTED);passwordField=field(overlay,"Пароль сети",18,207,444,64,true);
  button(overlay,"Показать / скрыть",18,274,216,44,TogglePassword);
  button(overlay,openNetwork?"Открытая сеть: ДА":"Открытая сеть: НЕТ",246,274,216,44,ToggleOpen,0x214258,!ssidLocked);
  if(openNetwork)lv_obj_add_state(passwordField,LV_STATE_DISABLED);
  dialogError=label(overlay,openNetwork?"Сеть без шифрования. Подключение по подтверждению":"WPA2/WPA3 Personal. Пароль от 8 символов",18,329,442,&boiler_font_14,AMBER);
  button(overlay,"Назад",18,387,144,50,Cancel);button(overlay,"Подключиться",174,387,288,50,Connect,0x157A92);
  makeKeyboard(460);activeField=ssidField?ssidField:passwordField;
}
void credentials(const AccessPoint* ap){credentialsFor(ap?ap->ssid:nullptr,ap&&ap->auth==0);}
void manualClock(){
  dismiss();dialogKind=2;overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  label(overlay,"Установка времени",18,23,306,&boiler_font_25);button(overlay,"Домой",350,14,112,50,Home);
  label(overlay,"Формат: ГГГГ-ММ-ДД ЧЧ:ММ",18,106,442,&boiler_font_19);
  timeField=field(overlay,"2026-10-27 18:40",18,153,444,24);activeField=timeField;
  label(overlay,panel_radio::zoneName(radio.zone),18,229,442,&boiler_font_19,SKY);
  dialogError=label(overlay,"После отключения питания нужна повторная установка\nили синхронизация по сети.",18,277,442,&boiler_font_14,MUTED);
  button(overlay,"Назад",18,370,144,52,Cancel);button(overlay,"Установить",174,370,288,52,ClockSave,0x157A92);
  makeKeyboard(450);lv_btnmatrix_set_map(keyboard,SYMBOLS);symbols=true;
}
void updateButtonState(lv_obj_t* object,bool enabled,bool visible=true);
void updateTargetEditor(){
  auto& editor=targetEditor;
  if(!editor.slider)return;
  const auto& measured=measurements[unsigned(editor.channel)];
  const bool fresh=measured.fresh(millis());
  char caption[48],value[24];
  if(fresh){
    temperature(value,sizeof(value),measured,millis());
    snprintf(caption,sizeof(caption),"%s °C",value);
  }else snprintf(caption,sizeof(caption),"%s",measured.count?"Данные устарели":"Нет данных");
  if(strcmp(lv_label_get_text(editor.reading),caption))lv_label_set_text(editor.reading,caption);

  panel_targets::Setting saved;
  const bool known=panel_targets::snapshot(unsigned(editor.channel),saved);
  if(known && !editor.edited){
    editor.original=saved;
    int initial=0;
    if(saved.configured)initial=saved.valueC;
    else if(fresh)initial=int(lroundf(measured.value));
    if(lv_slider_get_value(editor.slider)!=initial)lv_slider_set_value(editor.slider,initial,LV_ANIM_OFF);
  }
  const bool showValue=editor.edited || (known&&editor.original.configured);
  const int selected=lv_slider_get_value(editor.slider);
  if(showValue)snprintf(caption,sizeof(caption),"%d °C",selected);
  else snprintf(caption,sizeof(caption),"%s",known?"Не задана":"Нет данных");
  if(strcmp(lv_label_get_text(editor.selection),caption))lv_label_set_text(editor.selection,caption);
  const bool conflict=known && editor.edited && saved.revision!=editor.original.revision;
  panel_update::Snapshot ota;
  const bool ready=panel_update::snapshot(ota)&&ota.started&&!ota.busy&&!ota.bootPending;
  const bool editable=known&&ready&&!conflict&&!editor.saveError;
  updateButtonState(editor.slider,editable);
  updateButtonState(editor.decrease,editable&&selected>panel_targets::MIN_C);
  updateButtonState(editor.increase,editable&&selected<panel_targets::MAX_C);
  updateButtonState(editor.save,editable&&editor.edited);
  const char* hint=editor.saveError?editor.saveError:
    !known?"Настройка недоступна. Повторите позже.":
    conflict?"Настройка изменилась. Откройте регулятор снова.":
    !ready?"Сохранение недоступно во время обновления.":
    editor.edited?"Сохраните выбранную температуру.":
    saved.configured?"Значение сохранено на панели.":"Выберите температуру бегунком или кнопками.";
  if(strcmp(lv_label_get_text(dialogError),hint))error(hint);
}
void targetChanged(lv_event_t*){
  auto& editor=targetEditor;
  if(!editor.slider||lv_obj_has_state(editor.slider,LV_STATE_DISABLED))return;
  editor.edited=!editor.original.configured || lv_slider_get_value(editor.slider)!=editor.original.valueC;
  updateTargetEditor();
}
void adjustTarget(int delta){
  auto* slider=targetEditor.slider;
  if(!slider||lv_obj_has_state(slider,LV_STATE_DISABLED))return;
  lv_slider_set_value(slider,lv_slider_get_value(slider)+delta,LV_ANIM_OFF);
  targetChanged(nullptr);
}
void saveTarget(){
  auto& editor=targetEditor;
  updateTargetEditor();
  if(!editor.save||lv_obj_has_state(editor.save,LV_STATE_DISABLED))return;
  const auto result=panel_targets::save(unsigned(editor.channel),editor.original.revision,lv_slider_get_value(editor.slider));
  if(result==panel_targets::Result::Saved){closeDialog=true;return;}
  editor.saveError=result==panel_targets::Result::Conflict?"Настройка изменилась. Откройте регулятор снова.":
    "Запись не подтверждена. Откройте регулятор снова.";
  updateTargetEditor();
}
void openTargetEditor(unsigned channel){
  dismiss();dialogKind=3;overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  auto& editor=targetEditor;editor.channel=int(channel);
  label(overlay,SENSOR_NAMES[channel],24,30,310,&boiler_font_25);
  button(overlay,"Домой",350,18,106,56,Home);
  auto* actual=box(overlay,24,110,432,126);
  label(actual,"Измеренная температура",20,15,392,&boiler_font_19,MUTED);
  editor.reading=label(actual,"",20,55,392,&boiler_font_25);
  auto* target=box(overlay,24,260,432,300);
  label(target,channel==0?"Справочная температура":"Заданная температура",20,18,392,&boiler_font_19,MUTED);
  editor.selection=label(target,"",20,81,236,&boiler_font_25,SKY);
  editor.decrease=button(target,"-1 °C",260,64,68,64,TargetLess);
  editor.increase=button(target,"+1 °C",340,64,68,64,TargetMore);
  editor.slider=lv_slider_create(target);
  lv_obj_set_pos(editor.slider,30,178);lv_obj_set_size(editor.slider,370,16);
  lv_obj_set_ext_click_area(editor.slider,18);
  lv_obj_set_style_bg_color(editor.slider,c(LINE),LV_PART_MAIN);
  lv_obj_set_style_bg_color(editor.slider,c(SKY),LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(editor.slider,c(TXT),LV_PART_KNOB);
  lv_obj_set_style_pad_all(editor.slider,9,LV_PART_KNOB);
  lv_obj_clear_flag(editor.slider,LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_slider_set_range(editor.slider,panel_targets::MIN_C,panel_targets::MAX_C);
  char limit[16];snprintf(limit,sizeof(limit),"%d °C",int(panel_targets::MIN_C));
  label(target,limit,22,221,145,&boiler_font_14,MUTED);
  snprintf(limit,sizeof(limit),"%d °C",int(panel_targets::MAX_C));
  auto* upperLimit=label(target,limit,260,221,148,&boiler_font_14,MUTED);
  lv_obj_set_style_text_align(upperLimit,LV_TEXT_ALIGN_RIGHT,0);
  dialogError=label(overlay,"",24,channel==0?566:595,432,&boiler_font_19,MUTED);
  if(channel==0)button(overlay,"Пороги отопления",24,625,432,48,RoomLimits);
  button(overlay,"Назад",24,684,168,60,Cancel);
  editor.save=button(overlay,"Сохранить",204,684,252,60,TargetSave,0x245D55);
  updateTargetEditor();
  lv_obj_add_event_cb(editor.slider,targetChanged,LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_add_event_cb(editor.slider,targetChanged,LV_EVENT_RELEASED,nullptr);
}

const char* fuelStatusText(FuelLevelStatus status){
  switch(status){
    case FuelLevelStatus::BelowLower:return "Ниже нижнего";
    case FuelLevelStatus::Between:return "Между датчиками";
    case FuelLevelStatus::UpperReached:return "Верхний достигнут";
    case FuelLevelStatus::Inconsistent:return "Проверьте датчики";
    case FuelLevelStatus::Stale:return "Данные устарели";
    default:return "Нет данных";
  }
}
uint32_t fuelStatusColor(FuelLevelStatus status){
  switch(status){
    case FuelLevelStatus::Inconsistent:return RED;
    case FuelLevelStatus::BelowLower:case FuelLevelStatus::Stale:return AMBER;
    case FuelLevelStatus::UpperReached:return GREEN;
    case FuelLevelStatus::Between:return SKY;
    default:return MUTED;
  }
}
const char* reedStatusText(const ReedLevelReading& reading,uint32_t now){
  if(!reading.valid)return "Нет данных";
  if(!reading.fresh(now))return "Устарело";
  return reading.reached?"Достигнут":"Не достигнут";
}
void updateFuelLabels(FuelLevelLabels& labels,uint32_t now){
  const auto status=fuelLevel.status(now);
  if(labels.summary){lv_label_set_text(labels.summary,fuelStatusText(status));lv_obj_set_style_text_color(labels.summary,c(fuelStatusColor(status)),0);}
  const ReedLevelReading* readings[]={&fuelLevel.upper,&fuelLevel.lower};
  lv_obj_t* texts[]={labels.upper,labels.lower};lv_obj_t* dots[]={labels.upperDot,labels.lowerDot};
  for(unsigned i=0;i<2;i++){
    const auto& reading=*readings[i];
    const uint32_t color=!reading.valid?MUTED:!reading.fresh(now)?AMBER:reading.reached?GREEN:MUTED;
    if(texts[i]){lv_label_set_text(texts[i],reedStatusText(reading,now));lv_obj_set_style_text_color(texts[i],c(color),0);}
    if(dots[i])lv_obj_set_style_bg_color(dots[i],c(color),0);
  }
}
void updateFuel(){
  updateCanister();
  const uint32_t now=millis();updateFuelLabels(fuelCardLabels,now);updateFuelLabels(fuelDetailLabels,now);
  if(fuelDetailHint){
    const char* hint="Для оценки нужны два свежих показания";
    switch(fuelLevel.status(now)){
      case FuelLevelStatus::BelowLower:hint="Топливо ниже нижнего датчика";break;
      case FuelLevelStatus::Between:hint="Нижний достигнут, верхний ещё нет";break;
      case FuelLevelStatus::UpperReached:hint="Достигнуты оба уровня";break;
      case FuelLevelStatus::Inconsistent:hint="Верхний достигнут, а нижний - нет";break;
      case FuelLevelStatus::Stale:hint="Есть устаревшие показания датчиков";break;
      default:break;
    }
    lv_label_set_text(fuelDetailHint,hint);
  }
}
void fuelDetails(){closeDialog=true;pendingPage=2;view=Root;rebuild=true;}

const uint16_t DISPLAY_TIMEOUTS[]={0,30,60,120,300,600,1800};
void action(lv_event_t* e){
  ++clicks;const uintptr_t code=reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  Command cmd;
  if(code>=Ap0 && code<Ap0+MAX_AP){
    const unsigned i=unsigned(code-Ap0);if(i>=displayedApCount)return;
    const auto& ap=displayedAps[i];
    if(!ap.supported)info("Тип защиты","Эта версия поддерживает открытые сети и WPA2/WPA3 Personal. Корпоративные сети и WEP не поддерживаются.");
    else if(ap.saved){
      cmd.job=Job::Connect;cmd.useSaved=true;cmd.open=ap.auth==0;
      snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",ap.ssid);send(cmd);
    }else credentials(&ap);
    return;
  }
  if(code>=SavedEdit0&&code<SavedForget0+MAX_WIFI_PROFILES){
    const bool forget=code>=SavedForget0;
    const unsigned i=unsigned(code-(forget?SavedForget0:SavedEdit0));
    if(i>=displayedNetworkCount)return;
    const auto& target=displayedNetworks[i];
    if(forget)confirmNetwork(&target);
    else if(!target.open)credentialsFor(target.ssid,false);
    return;
  }
  if(code>=Temp0 && code<Temp0+CHANNELS){
    openTargetEditor(unsigned(code-Temp0));return;
  }
  if(code>=Timeout0 && code<=Timeout6){cmd.job=Job::SetDisplayTimeout;cmd.displayTimeoutSec=DISPLAY_TIMEOUTS[code-Timeout0];send(cmd);return;}
  switch(code){
    case Home:closeDialog=true;pendingPage=0;slideDirection=-1;view=0;break;
    case Back:view=page==PAGE_SETTINGS?parentMenuView(view):page==1&&view>=6?5:Root;rebuild=true;break;
    case HeatingPage:closeDialog=true;pendingPage=1;view=5;break;
    case RoomLimits:editRoomLimits();break;
    case RoomSave:saveRoomLimits();break;
    case ViewFloors:view=6;rebuild=true;break;
    case ViewRadiators:view=7;rebuild=true;break;
    case RoomCooler:moveRoomLimits(-1);break;
    case RoomWarmer:moveRoomLimits(1);break;
    case BoilerEdit:editBoiler();break;
    case BoilerSave:saveBoiler();break;
    case TargetLess:adjustTarget(-1);break;
    case TargetMore:adjustTarget(1);break;
    case TargetSave:saveTarget();break;
    case Connections:view=ConnectionsView;rebuild=true;break;
    case HaStatus:view=HaView;rebuild=true;break;
    case ServiceMenu:view=ServiceView;rebuild=true;break;
    case UpdateMenu:closeDialog=true;pendingPage=PAGE_SETTINGS;view=UpdateView;rebuild=true;break;
    case UpdateCheck:
      if(!panel_update::requestCheck())info("Проверка не началась","Дождитесь завершения текущей операции и повторите.");
      updateUpdateLabels();break;
    case UpdateInstall:confirmUpdate(false);break;
    case UpdateRollback:confirmUpdate(true);break;
    case UpdateCancel:
      if(!panel_update::requestCancel())info("Отмена недоступна","Панель уже завершает установку. Дождитесь перезапуска.");
      updateUpdateLabels();break;
    case UpdateConfirm:{
      const bool accepted=confirmedUpdateVersion[0]&&(confirmUpdateRollback?
        panel_update::requestRollback(confirmedUpdateVersion):panel_update::requestInstall(confirmedUpdateVersion));
      if(accepted){closeDialog=true;pendingPage=PAGE_SETTINGS;view=UpdateView;rebuild=true;}
      else error("Версия или состояние изменились.\nЗакройте окно и проверьте снова.");
      break;
    }
    case StorageMenu:view=StorageView;rebuild=true;break;
    case DiagnosticsMenu:view=DiagnosticsView;rebuild=true;break;
    case AboutMenu:view=AboutView;rebuild=true;break;
    case WifiMore:view=WifiMoreView;rebuild=true;break;
    case ConfirmDisconnect:cmd.job=Job::Disconnect;if(send(cmd))closeDialog=true;break;
    case ConfirmForget:
      if(confirmedForget.ssid[0]){
        cmd.job=Job::Forget;cmd.open=confirmedForget.open;
        snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",confirmedForget.ssid);
        if(send(cmd))closeDialog=true;
      }break;
    case Display:view=4;rebuild=true;break;
    case SdFormatApprove:{
      panel_update::Snapshot update;panel_update::snapshot(update);
      if(update.busy||update.bootPending)info("Обновление панели","Дождитесь завершения обновления перед форматированием карты.");
      else panel_storage::confirmFormatPhysically();
      updateStorageLabel();break;
    }
    case SdFormatCancel:panel_storage::cancelFormatPhysically();updateStorageLabel();break;
    case Wifi:view=1;rebuild=true;break;
    case Ble:view=2;rebuild=true;break;
    case WifiScan:cmd.job=Job::WifiScan;send(cmd);break;
    case WifiManual:credentials(nullptr);break;
    case WifiDisconnect:confirmNetwork();break;
    case BleScan:cmd.job=Job::BleScan;send(cmd);break;
    case ManualClock:manualClock();break;
    case Cancel:closeDialog=true;break;
    case TogglePassword:lv_textarea_set_password_mode(passwordField,!lv_textarea_get_password_mode(passwordField));break;
    case ToggleOpen:
      openNetwork=!openNetwork;
      if(openNetwork){lv_textarea_set_text(passwordField,"");lv_obj_add_state(passwordField,LV_STATE_DISABLED);error("Открытая сеть: ДА. Пароль не используется");}
      else{lv_obj_clear_state(passwordField,LV_STATE_DISABLED);error("Открытая сеть: НЕТ. Введите пароль");}
      {auto* caption=lv_obj_get_child(lv_event_get_target(e),0);if(caption)lv_label_set_text(caption,openNetwork?"Открытая сеть: ДА":"Открытая сеть: НЕТ");}break;
    case Connect:{
      const char* ssid=ssidLocked?selectedSsid:lv_textarea_get_text(ssidField);
      const char* key=lv_textarea_get_text(passwordField);
      if(!ssid[0] || strlen(ssid)>32){error("Имя сети: от 1 до 32 байт");break;}
      if(!passwordValid(key,openNetwork)){error("Пароль: 8-63 символа или 64 шестнадцатеричных знака");break;}
      cmd.job=Job::Connect;cmd.open=openNetwork;snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",ssid);snprintf(cmd.password,sizeof(cmd.password),"%s",key);
      if(send(cmd)){closeDialog=true;view=1;rebuild=true;}break;
    }
    case ClockSave:
      cmd.job=Job::SetClock;snprintf(cmd.input,sizeof(cmd.input),"%s",lv_textarea_get_text(timeField));
      {time_t parsed;if(!parseLocalTime(cmd.input,parsed)){error("Проверьте дату. Формат: 2026-10-27 18:40");break;}}
      if(send(cmd)){closeDialog=true;view=3;rebuild=true;}break;
    case Zone0:case Zone1:case Zone2:cmd.job=Job::Zone;cmd.zone=uint8_t(code-Zone0);send(cmd);break;
    case ServiceRefresh:rebuild=true;break;
    case FuelOverview:fuelDetails();break;
    case EquipmentDhw:case EquipmentElectric:case EquipmentDiesel:case EquipmentFuel:
      closeDialog=true;pendingPage=1;view=int(code-EquipmentDhw)+1;break;
    default:break;
  }
}
void updateClock(){
  if(!clockLabel)return;
  const time_t now=time(nullptr);char t[16]="--:--",d[16]="--.--.----";tm local={};
  if(now>=time_t(1704067200)&&localtime_r(&now,&local)){
    strftime(t,sizeof(t),"%H:%M",&local);strftime(d,sizeof(d),"%d.%m.%Y",&local);
  }
  if(strcmp(lv_label_get_text(clockLabel),t))lv_label_set_text(clockLabel,t);
  if(dateLabel&&strcmp(lv_label_get_text(dateLabel),d))lv_label_set_text(dateLabel,d);
}
const char* currentTitle(){
  if(page==1&&view>0)return view==1?"Бойлер":view==2?"Электрокотёл":view==3?"Отопитель":view==4?"Подогрев топлива":view==5?"Автоматика":"Контуры";
  if(page==PAGE_SETTINGS)switch(view){
    case ConnectionsView:return "Подключения";case WifiView:return "Wi-Fi";case BleView:return "Bluetooth";
    case DisplayView:return "Экран";case HaView:return "Home Assistant";
    case ServiceView:return "Сервис";case StorageView:return "microSD";case DiagnosticsView:return "Диагностика";
    case AboutView:return "О панели";case WifiMoreView:return "Действия Wi-Fi";case UpdateView:return "Обновление";default:break;
  }
  return page==0?"Котельная":PAGES[page];
}
void makeHeader(){
  if(header)lv_obj_del(header);
  clockLabel=dateLabel=headerTitle=pairBadge=pairDigits=nullptr;
  header=box(lv_scr_act(),0,0,480,100,BG,0);lv_obj_set_style_border_width(header,0,0);
  const bool nested=view!=Root&&(page==1||page==PAGE_SETTINGS);
  if(nested)button(header,"<",16,18,56,56,Back);
  headerTitle=label(header,currentTitle(),nested?88:24,32,nested?250:315,&boiler_font_25);
  if(page==0){
    clockLabel=label(header,"--:--",334,18,122,&boiler_font_25,SKY);lv_obj_set_style_text_align(clockLabel,LV_TEXT_ALIGN_RIGHT,0);
    dateLabel=label(header,"--.--.----",334,55,122,&boiler_font_19,MUTED);lv_obj_set_style_text_align(dateLabel,LV_TEXT_ALIGN_RIGHT,0);updateClock();
    pairBadge=box(header,196,16,132,66,0x245D55,12);lv_obj_clear_flag(pairBadge,LV_OBJ_FLAG_CLICKABLE);
    auto* caption=label(pairBadge,"Код",12,4,108,&boiler_font_14,TXT);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
    pairDigits=label(pairBadge,"",8,28,116,&boiler_font_25,TXT);lv_obj_set_style_text_align(pairDigits,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_flag(pairBadge,LV_OBJ_FLAG_HIDDEN);
  }
  else button(header,"Домой",350,18,106,56,Home);
}
void makeFooter(){
  if(footer)lv_obj_del(footer);
  footer=box(lv_scr_act(),0,773,480,27,BG,0);lv_obj_set_style_border_width(footer,0,0);
  // Passive dots: no bottom navigation buttons.
  for(int i=0;i<4;i++){auto* dot=box(footer,204+i*20,8,i==page?16:7,6,i==page?SKY:LINE,3);lv_obj_set_style_border_width(dot,0,0);lv_obj_clear_flag(dot,LV_OBJ_FLAG_CLICKABLE);}
}
lv_obj_t* menuRow(lv_obj_t* parent,const char* title,const char* subtitle,int y,uintptr_t code,int h=116){
  auto* row=box(parent,24,y,432,h);lv_obj_set_style_bg_color(row,c(0x293B3F),LV_STATE_PRESSED);
  lv_obj_add_event_cb(row,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(code));
  label(row,title,20,22,350,&boiler_font_25);
  if(subtitle&&*subtitle)label(row,subtitle,20,64,350,&boiler_font_19,MUTED);
  label(row,">",386,(h-25)/2,25,&boiler_font_25,MUTED);return row;
}
void confirmNetwork(const SavedNetwork* target){
  dismiss();const bool forget=target!=nullptr;if(forget)confirmedForget=*target;
  dialogKind=3;overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  label(overlay,forget?"Забыть сеть?":"Отключить Wi-Fi?",24,70,432,&boiler_font_25);
  if(forget){
    auto* name=label(overlay,confirmedForget.ssid,24,138,432,&boiler_font_25,SKY);
    lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,38);
    label(overlay,confirmedForget.open?"Открытая сеть будет удалена из сохранённых.":"Пароль этой сети будет удалён. Для подключения понадобится ввести его снова.",24,204,432,&boiler_font_19,MUTED);
  }else label(overlay,"Веб-панель и Home Assistant потеряют связь с устройством.",24,160,432,&boiler_font_19,MUTED);
  button(overlay,"Отмена",24,350,432,60,Cancel);
  button(overlay,forget?"Забыть сеть":"Отключить",24,430,432,60,forget?ConfirmForget:ConfirmDisconnect,0x704443);
  dialogError=label(overlay,"",24,540,432,&boiler_font_19,AMBER);
}
void confirmUpdate(bool rollback){
  panel_update::Snapshot snapshot;
  if(!panel_update::snapshot(snapshot)||!snapshot.started||!snapshot.supported||snapshot.busy||snapshot.bootPending||
     (rollback?!snapshot.rollbackAvailable:!snapshot.available)){
    info("Обновление панели","Состояние изменилось. Проверьте доступную версию ещё раз.");return;
  }
  const char* version=rollback?snapshot.previousVersion:snapshot.latestVersion;
  if(!version[0])return;
  dismiss();confirmUpdateRollback=rollback;
  // Keep this exact target until the explicit confirmation is accepted or dismissed.
  snprintf(confirmedUpdateVersion,sizeof(confirmedUpdateVersion),"%s",version);
  dialogKind=5;overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  label(overlay,rollback?"Вернуть предыдущую версию?":"Установить обновление?",24,60,432,&boiler_font_25);
  label(overlay,confirmedUpdateVersion,24,145,432,&boiler_font_25,SKY);
  label(overlay,"После установки панель\nперезапустится. Дождитесь\nзавершения, сохраняя питание.",24,214,432,&boiler_font_19,MUTED);
  button(overlay,"Отмена",24,370,432,60,Cancel);
  button(overlay,rollback?"Вернуть версию":"Установить",24,450,432,60,UpdateConfirm,0x245D55);
  dialogError=label(overlay,"",24,554,432,&boiler_font_19,AMBER);
}
bool homeUpdateVisible(){
  return updateState.started&&updateState.supported&&(updateState.available||updateState.busy||updateState.bootPending||
    updateState.phase==panel_update::Phase::Success||updateState.phase==panel_update::Phase::RolledBack);
}
void passive(lv_obj_t* object){
  lv_obj_clear_flag(object,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(object,LV_OBJ_FLAG_GESTURE_BUBBLE);
}
// Decorative vector icons. No sensor state, weather or actuator state is inferred.
// Coordinates match the embedded web SVGs; line point arrays have static lifetime.
lv_obj_t* iconRect(lv_obj_t* parent,int x,int y,int w,int h,int radius=5){
  auto* o=box(parent,x,y,w,h,CARD,radius);
  lv_obj_set_style_border_width(o,3,0);lv_obj_set_style_border_color(o,c(SKY),0);passive(o);return o;
}
void iconLine(lv_obj_t* parent,const lv_point_t* points,unsigned count){
  auto* o=lv_line_create(parent);lv_line_set_points(o,points,count);
  lv_obj_set_style_line_color(o,c(SKY),0);lv_obj_set_style_line_width(o,3,0);
  lv_obj_set_style_line_rounded(o,true,0);passive(o);
}
void temperatureIcon(lv_obj_t* tile,unsigned channel){
  auto* root=box(tile,44,156,120,108,CARD,0);tempIcon[channel]=root;
  lv_obj_set_style_border_width(root,0,0);passive(root);
  if(channel==0){
    iconRect(root,82,16,10,27,2);
    iconRect(root,24,44,72,56,6);
    static const lv_point_t roof[]={{10,46},{60,8},{110,46}};
    iconLine(root,roof,3);
    auto* door=iconRect(root,56,65,22,35,3);lv_obj_set_style_bg_color(door,c(0x245D55),0);
    auto* window=iconRect(root,35,61,12,14,2);lv_obj_set_style_bg_color(window,c(0x245D55),0);
  }else{
    iconRect(root,18,16,40,40,LV_RADIUS_CIRCLE);
    static const lv_point_t rays[][2]={{{38,3},{38,9}},{{8,36},{2,36}},{{64,10},{59,15}},{{10,10},{15,15}},{{67,36},{74,36}},{{10,62},{15,57}}};
    for(const auto& ray:rays)iconLine(root,ray,2);
    // Opaque backing occludes the sun behind a single rounded cloud contour.
    auto* cover=box(root,32,40,78,58,CARD,0);lv_obj_set_style_border_width(cover,0,0);passive(cover);
    static const lv_point_t cloud[]={{34,94},{25,91},{20,84},{18,76},{20,68},{26,62},{34,60},{37,49},{44,40},{54,35},{65,34},{76,37},{84,44},{88,54},{96,54},{104,58},{110,65},{112,74},{110,84},{104,91},{96,94},{34,94}};
    iconLine(root,cloud,sizeof(cloud)/sizeof(cloud[0]));
  }
}
void coolantIcon(lv_obj_t* tile){
  auto* root=box(tile,161,9,32,24,CARD,0);
  lv_obj_set_style_border_width(root,0,0);passive(root);
  auto* radiator=iconRect(root,4,4,24,15,3);
  lv_obj_set_style_border_width(radiator,2,0);
  for(int x:{10,15,20}){
    auto* fin=box(root,x,7,2,9,SKY,1);
    lv_obj_set_style_border_width(fin,0,0);passive(fin);
  }
  static const lv_point_t inlet[]={{2,2},{2,7},{4,7}};
  static const lv_point_t outlet[]={{27,15},{30,15},{30,21}};
  iconLine(root,inlet,3);iconLine(root,outlet,3);
}
void canister(lv_obj_t* heading){
  fuelCanister.root=box(heading,158,1,40,40,CARD,0);
  lv_obj_set_style_border_width(fuelCanister.root,0,0);passive(fuelCanister.root);
  fuelCanister.handle=box(fuelCanister.root,17,1,15,9,CARD,2);
  fuelCanister.cap=box(fuelCanister.root,7,3,8,4,CARD,1);
  fuelCanister.body=box(fuelCanister.root,5,8,29,30,CARD,3);
  for(auto* o:{fuelCanister.handle,fuelCanister.cap,fuelCanister.body}){
    lv_obj_set_style_border_width(o,2,0);passive(o);
  }
  fuelCanister.fill=box(fuelCanister.body,1,1,23,CANISTER_FILL_HEIGHT,GREEN,2);
  lv_obj_set_style_border_width(fuelCanister.fill,0,0);passive(fuelCanister.fill);
  // LVGL stores the point array by reference, so both arrays have static lifetime.
  static const lv_point_t crossA[]={{0,0},{10,11}},crossB[]={{10,0},{0,11}};
  fuelCanister.crossA=lv_line_create(fuelCanister.body);
  fuelCanister.crossB=lv_line_create(fuelCanister.body);
  lv_line_set_points(fuelCanister.crossA,crossA,2);lv_line_set_points(fuelCanister.crossB,crossB,2);
  for(auto* o:{fuelCanister.crossA,fuelCanister.crossB}){
    lv_obj_set_pos(o,7,7);lv_obj_set_style_line_width(o,2,0);
    lv_obj_set_style_line_color(o,c(CARD),0);lv_obj_set_style_line_rounded(o,true,0);passive(o);
  }
  fuelCanister.glyph=label(fuelCanister.body,"?",0,3,25,&boiler_font_14,MUTED);
  lv_obj_set_style_text_align(fuelCanister.glyph,LV_TEXT_ALIGN_CENTER,0);passive(fuelCanister.glyph);
  fuelCanister.status=label(heading,"Нет уровня",16,27,138,&boiler_font_14,MUTED);
  updateCanister();
}
void updateCanister(){
  if(!fuelCanister.root)return;
  const uint32_t now=millis();
  int height=0;uint32_t ink=MUTED;const char* mark="?";const char* status="Нет уровня";
  // These are normalized height states shared by the display, HTTP and HA.
  // With both inputs set to ReedNC: open/open=full; upper closed=half;
  // both closed=empty; lower closed alone=inconsistent. Missing data != full.
  switch(fuelLevel.status(now)){
    case FuelLevelStatus::UpperReached:height=CANISTER_FILL_HEIGHT;ink=GREEN;mark="";status="";break;
    case FuelLevelStatus::Between:height=CANISTER_FILL_HEIGHT/2;ink=AMBER;mark="";status="";break;
    case FuelLevelStatus::BelowLower:ink=RED;mark="!";status="";break;
    case FuelLevelStatus::Inconsistent:ink=AMBER;status="Ошибка уровня";break;
    case FuelLevelStatus::Stale:ink=AMBER;status="Устарело";break;
    default:break;
  }
  for(auto* o:{fuelCanister.handle,fuelCanister.cap,fuelCanister.body})lv_obj_set_style_border_color(o,c(ink),0);
  if(height){
    lv_obj_set_height(fuelCanister.fill,height);lv_obj_set_y(fuelCanister.fill,1+CANISTER_FILL_HEIGHT-height);
    lv_obj_set_style_bg_color(fuelCanister.fill,c(ink),0);lv_obj_clear_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN);
  }else lv_obj_add_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN);
  for(auto* o:{fuelCanister.crossA,fuelCanister.crossB}){
    if(height){lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);lv_obj_set_style_line_color(o,c(height==CANISTER_FILL_HEIGHT?CARD:ink),0);}
    else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text(fuelCanister.glyph,mark);lv_obj_set_style_text_color(fuelCanister.glyph,c(ink),0);
  lv_label_set_text(fuelCanister.status,status);lv_obj_set_style_text_color(fuelCanister.status,c(ink),0);
}
void bindReading(lv_obj_t* object,unsigned channel){
  const uintptr_t actionId=channel==0?uintptr_t(RoomLimits):uintptr_t(Temp0+channel);
  lv_obj_add_event_cb(object,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(actionId));
}
void pairedReading(lv_obj_t* card,unsigned channel,int x,int y){
  auto* row=box(card,x,y,206,125,CARD,0);
  lv_obj_set_style_border_width(row,0,0);
  lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);
  lv_obj_set_style_bg_color(row,c(0x26373B),LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(row,LV_OPA_COVER,LV_STATE_PRESSED);
  bindReading(row,channel);
  label(row,SENSOR_NAMES[channel],16,0,135,&boiler_font_19);
  label(row,"°C",158,0,36,&boiler_font_19,MUTED);
  tempValue[channel]=label(row,"--.-",16,28,178,&boiler_font_55);
  tempState[channel]=label(row,"Нет данных",16,104,178,&boiler_font_14,MUTED);
}
void home(){
  for(unsigned i=0;i<2;i++){
    auto* tile=box(body,24+int(i)*224,12,208,300);
    bindReading(tile,i);
    lv_obj_set_style_border_color(tile,c(SKY),LV_STATE_PRESSED);
    label(tile,SENSOR_NAMES[i],18,22,172,&boiler_font_19,MUTED);
    tempValue[i]=label(tile,"--.-",16,50,178,&boiler_font_55);
    label(tile,"°C",18,128,172,&boiler_font_19,MUTED);
    temperatureIcon(tile,i);
    tempState[i]=label(tile,"Нет данных",18,269,172,&boiler_font_14,MUTED);
  }
  auto* coolant=box(body,24,334,208,300);
  passive(coolant);
  label(coolant,"Теплоноситель",16,8,141,&boiler_font_19,MUTED);
  coolantIcon(coolant);
  pairedReading(coolant,2,0,48);
  pairedReading(coolant,4,0,173);

  auto* fuel=box(body,248,334,208,300);
  passive(fuel);
  auto* heading=box(fuel,0,0,206,48,CARD,0);
  lv_obj_set_style_border_width(heading,0,0);
  lv_obj_set_style_bg_opa(heading,LV_OPA_TRANSP,0);
  lv_obj_add_event_cb(heading,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(FuelOverview)));
  label(heading,"Топливо",16,0,138,&boiler_font_19,MUTED);
  canister(heading);
  pairedReading(fuel,3,0,48);
  pairedReading(fuel,5,0,173);
  if(homeUpdateVisible()){
    homeUpdateNotice=box(body,24,638,432,34,BG,8);lv_obj_set_style_border_width(homeUpdateNotice,0,0);
    lv_obj_add_event_cb(homeUpdateNotice,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(UpdateMenu));
    homeUpdateText=label(homeUpdateNotice,"",8,8,416,&boiler_font_14,SKY);
    lv_label_set_long_mode(homeUpdateText,LV_LABEL_LONG_DOT);lv_obj_set_height(homeUpdateText,20);
  }
}

void setLabel(lv_obj_t* labelObj,const char* text){
  if(labelObj && strcmp(lv_label_get_text(labelObj),text))lv_label_set_text(labelObj,text);
}
const char* requested(panel_heating::Flag value){
  if(value==panel_heating::Flag::Yes)return "Включить";
  if(value==panel_heating::Flag::No)return "Выключить";
  return "Нет данных";
}
void heatingTemperature(char* out,size_t capacity,float value){
  if(isfinite(value))snprintf(out,capacity,"%.1f °C",double(value));
  else snprintf(out,capacity,"Нет данных");
}
void showHeatingRequests(){
  if(!autoValues[0]&&!waterReading&&!electricValues[0]&&!loopRooms[0])return;
  panel_heating_runtime::Snapshot s{};
  panel_heating_runtime::snapshot(s);
  using namespace panel_heating;
  const auto& in=s.input;const auto& out=s.output;
  char power[20],text[144];
  if(out.stages<0)snprintf(power,sizeof(power),"Нет данных");
  else snprintf(power,sizeof(power),"%d кВт",int(out.stages)*3);
  if(autoValues[0]){
    setLabel(autoValues[0],in.presence==Presence::Home?"Дома":in.presence==Presence::Away?"Никого нет":"Нет данных");
    setLabel(autoValues[1],in.season==Season::Winter?"Зима":in.season==Season::Summer?"Лето":"Не задан");
    setLabel(autoValues[2],out.demand==Flag::Yes?"Нужен нагрев":out.demand==Flag::No?"Нет запроса":"Нет данных");
    setLabel(autoValues[3],out.source==Source::Diesel?"Дизель":out.source==Source::Electric?"Электрокотёл":out.source==Source::None?(out.demand==Flag::Yes?"Нет источника":"Нет запроса"):"Нет данных");
    setLabel(autoValues[4],power);
    setLabel(autoValues[5],requested(out.tankPower));
    setLabel(autoValues[6],requested(out.coilPump));
    setLabel(autoValues[7],out.alarm==Flag::Yes?"Авария":out.alarm==Flag::No?"Нет аварии":"Нет данных");
    lv_obj_set_style_text_color(autoValues[7],c(out.alarm==Flag::Yes?RED:TXT),0);
  }
  if(loopRooms[0]){
    const unsigned first=view==6?0:8;
    for(unsigned row=0;row<8 && loopRooms[row];row++){
      const unsigned index=first+row,room=s.roomForLoop[index];
      if(!room || room>ROOM_COUNT)snprintf(text,sizeof(text),"Без комнаты");
      else if(isfinite(in.roomC[room-1]))snprintf(text,sizeof(text),"%u  /  %.1f °C",room,double(in.roomC[room-1]));
      else snprintf(text,sizeof(text),"%u  /  нет данных",room);
      setLabel(loopRooms[row],text);
      const auto request=out.loop[index];
      setLabel(loopRequests[row],!room?"-":request==Flag::Yes?"Открыть":request==Flag::No?"Закрыть":"Нет данных");
    }
  }
  if(waterReading){
    heatingTemperature(text,sizeof(text),in.tankC);setLabel(waterReading,text);
    snprintf(text,sizeof(text),"Запрос питания: %s\nНасос змеевика: %s",requested(out.tankPower),requested(out.coilPump));
    setLabel(dhwRequests,text);
  }
  if(electricValues[0]){
    heatingTemperature(text,sizeof(text),in.supplyC);setLabel(electricValues[0],text);
    heatingTemperature(text,sizeof(text),in.returnC);setLabel(electricValues[1],text);
    setLabel(electricValues[2],power);
  }
}
void heatingPage(){
  label(body,"Расчётные запросы",24,12,430,&boiler_font_25);
  label(body,"Состояние нагрузок не подтверждено",24,52,432,&boiler_font_14,MUTED);
  auto* card=box(body,24,91,432,402);
  const char* names[]={"Переключатель","Сезон","Помещения","Источник","Мощность ТЭНов","Питание бойлера","Насос змеевика","Авария источника"};
  for(unsigned i=0;i<8;i++){
    const int y=18+int(i)*48;
    label(card,names[i],15,y,204,&boiler_font_19,MUTED);
    autoValues[i]=label(card,"Нет данных",219,y,199,&boiler_font_19);
    lv_obj_set_style_text_align(autoValues[i],LV_TEXT_ALIGN_RIGHT,0);
  }
  button(body,"Пороги отопления",24,511,238,62,RoomLimits);
  button(body,"Контуры",272,511,184,62,ViewFloors);
  label(body,"Назначение контуров и параметры воды доступны в веб-интерфейсе.",24,593,432,&boiler_font_14,MUTED);
}
void heatingLoopsPage(){
  const bool floors=view==6;
  button(body,"Тёплый пол",24,14,210,58,ViewFloors,floors?0x245D55:0x26373B);
  button(body,"Батареи",246,14,210,58,ViewRadiators,floors?0x26373B:0x245D55);
  label(body,"Контур",32,96,88,&boiler_font_14,MUTED);
  label(body,"Комната / °C",126,96,174,&boiler_font_14,MUTED);
  label(body,"Запрос",316,96,134,&boiler_font_14,MUTED);
  const unsigned count=floors?8:6;
  for(unsigned row=0;row<count;row++){
    const int y=128+int(row)*59;
    auto* line=box(body,24,y,432,53);
    char number[8];snprintf(number,sizeof(number),"%u",row+1);
    label(line,number,10,13,82,&boiler_font_19);
    loopRooms[row]=label(line,"Без комнаты",102,13,185,&boiler_font_19);
    loopRequests[row]=label(line,"-",292,13,132,&boiler_font_19);
  }
  label(body,"Настройте комнаты и контуры в разделе Автоматика веб-панели.",24,617,432,&boiler_font_14,MUTED);
}
void updateEquipmentReadings(){
  showHeatingRequests();
  if(heaterPinLabels[0]){
    panel_config::State config;
    const bool known=panel_config::snapshot(config);
    for(unsigned i=0;i<HEATER_COUNT;i++){
      const auto& output=config.item[unsigned(ELECTRIC_HEATERS[i].channel)];
      char pin[32];
      if(!known)snprintf(pin,sizeof(pin),"Нет данных");
      else if(output.type==panel_config::Type::Relay)snprintf(pin,sizeof(pin),"GPIO %d",int(output.gpio));
      else snprintf(pin,sizeof(pin),"Без выхода");
      if(strcmp(lv_label_get_text(heaterPinLabels[i]),pin))lv_label_set_text(heaterPinLabels[i],pin);
    }
  }
  if(boilerOnLabel&&boilerOffLabel){
    panel_boiler::Settings settings;const bool ok=panel_boiler::snapshot(settings,equipmentBoilerTarget());
    char on[80],off[80];
    if(ok&&settings.configured){snprintf(on,sizeof(on),"%u °C и ниже",unsigned(settings.onC));snprintf(off,sizeof(off),"%u °C и выше",unsigned(settings.offC));}
    else{snprintf(on,sizeof(on),"%s",ok?"Не задано":"Нет данных");snprintf(off,sizeof(off),"%s",on);}
    if(strcmp(lv_label_get_text(boilerOnLabel),on))lv_label_set_text(boilerOnLabel,on);
    if(strcmp(lv_label_get_text(boilerOffLabel),off))lv_label_set_text(boilerOffLabel,off);
  }
}
lv_obj_t* equipmentInfoCard(lv_obj_t* parent,const char* title,int x,int width){
  auto* card=box(parent,x,0,width,84);
  label(card,title,20,16,width-40,&boiler_font_19);
  return label(card,"Нет данных",20,48,width-40,&boiler_font_14,MUTED);
}
void control(){
  const char* names[]={"Бойлер","Электрокотёл","Дизельный отопитель","Подогрев топлива"};
  for(unsigned i=0;i<4;i++)menuRow(body,names[i],"Не подключён",16+int(i)*128,EquipmentDhw+i,109);
  menuRow(body,"Автоматика","Запросы и пороги помещений",528,HeatingPage,109);
}
void fuelTemperatures(int y){
  auto* card=box(body,24,y,432,172);
  passive(card);
  label(card,"Температура топлива",16,7,398,&boiler_font_19,MUTED);
  pairedReading(card,3,0,40);
  pairedReading(card,5,216,40);
}
lv_obj_t* unavailableHeatButton(const char* caption,int x,int y,int width,int height){
  auto* heat=button(body,caption,x,y,width,height,0,0x26373B,false);
  // Keep the unavailable action legible without using the active action color.
  lv_obj_set_style_opa(heat,LV_OPA_COVER,LV_STATE_DISABLED);
  lv_obj_set_style_color_filter_opa(heat,LV_OPA_TRANSP,LV_STATE_DISABLED);
  lv_obj_set_style_text_color(lv_obj_get_child(heat,0),c(MUTED),0);
  return heat;
}
void boilerThresholds(int y){
  auto* limits=box(body,24,y,432,210);
  label(limits,"Сохранённый порог включения",20,16,390,&boiler_font_19,MUTED);
  boilerOnLabel=label(limits,"",20,52,390,&boiler_font_25);
  label(limits,"Сохранённый порог выключения",20,112,390,&boiler_font_19,MUTED);
  boilerOffLabel=label(limits,"",20,148,390,&boiler_font_25);
}
void equipmentPage(){
  if(view==1){
    auto* water=box(body,24,16,432,120);
    label(water,"Температура воды в бойлере",20,18,390,&boiler_font_19,MUTED);
    waterReading=label(water,"Нет данных",20,58,390,&boiler_font_25);
    unavailableHeatButton("Включить бойлер",24,160,432,58);
    dhwRequests=label(body,"",24,230,432,&boiler_font_19,MUTED);
    boilerThresholds(307);
    button(body,"Изменить пороги",24,552,432,64,BoilerEdit);
    label(body,"Уставки змеевика задаются в разделе Автоматика веб-панели.",24,629,432,&boiler_font_14,MUTED);
    return;
  }
  if(view==2){
    for(unsigned i=0;i<HEATER_COUNT;i++){
      auto* heater=unavailableHeatButton(ELECTRIC_HEATERS[i].name,24+int(i)*148,16,136,82);
      lv_obj_align(lv_obj_get_child(heater,0),LV_ALIGN_TOP_MID,0,8);
      heaterPinLabels[i]=label(heater,"",0,0,124,&boiler_font_14,MUTED);
      lv_obj_set_style_text_align(heaterPinLabels[i],LV_TEXT_ALIGN_CENTER,0);
      lv_obj_align(heaterPinLabels[i],LV_ALIGN_BOTTOM_MID,0,-8);
    }
    label(body,"Расчёт по подаче и обратке.\nКонтроллер ТЭНов не подключён.",24,109,432,&boiler_font_19,MUTED);
    boilerThresholds(176);
    button(body,"Изменить пороги",24,410,432,58,BoilerEdit);
    auto* list=box(body,24,510,432,88,BG,0);lv_obj_set_style_border_width(list,0,0);
    electricValues[0]=equipmentInfoCard(list,"Подача",0,136);
    electricValues[1]=equipmentInfoCard(list,"Обратка",148,136);
    electricValues[2]=equipmentInfoCard(list,"Запрос",296,136);
    label(body,"Уставки подачи и обратки задаются в разделе Автоматика веб-панели.",24,617,432,&boiler_font_14,MUTED);
    return;
  }
  auto* state=box(body,24,16,432,168);
  label(state,"Контроллер",20,22,390,&boiler_font_19,MUTED);
  label(state,"Не подключён",20,68,390,&boiler_font_25);
  label(state,"Управление недоступно",20,120,390,&boiler_font_14,MUTED);
  if(view==4){
    fuelTemperatures(216);
    label(body,"До 80 °C — требование проекта,\nне действующая уставка.",24,420,432,&boiler_font_19,MUTED);
    menuRow(body,"Уровень топлива","Открыть показания",532,FuelOverview,112);
  }else{
    label(body,"Дизельный отопитель",24,224,432,&boiler_font_19,MUTED);
    label(body,"Данные появятся после подключения\nисполнительного контроллера.",24,282,432,&boiler_font_19,MUTED);
  }
}
void fuelPage(){
  fuelTemperatures(16);
  auto* level=box(body,24,196,432,130);
  label(level,"Уровень",20,20,390,&boiler_font_19,MUTED);
  fuelCardLabels.summary=label(level,"Нет данных",20,63,390,&boiler_font_25);
  for(unsigned n=0;n<2;n++){
    auto* row=box(body,24,346+int(n)*92,432,76);
    label(row,n==0?"Нижний датчик":"Верхний датчик",16,13,210,&boiler_font_19);
    auto* value=label(row,"Нет данных",16,44,390,&boiler_font_14,MUTED);
    if(n==0)fuelCardLabels.lower=value;else fuelCardLabels.upper=value;
  }
  menuRow(body,"Подогрев топлива","Не подключён",548,EquipmentFuel,100);
}
void settings(){
  menuRow(body,"Подключения","Wi-Fi, Bluetooth, Home Assistant",16,Connections,122);
  menuRow(body,"Экран","Сон и пробуждение",159,Display,122);
  menuRow(body,"Сервис","Обновление, карта и диагностика",302,ServiceMenu,122);
}
void connectionsPage(){
  menuRow(body,"Wi-Fi",radio.connected?"Подключён":"Не подключён",16,Wifi,122);
  menuRow(body,"Bluetooth","Поиск устройств рядом",159,Ble,122);
  panel_ha::Snapshot ha;panel_ha::snapshot(ha);
  menuRow(body,"Home Assistant",!ha.ready?"Недоступен":!ha.enabled?"Выключен":ha.connected?"Подключён":"Нет связи",302,HaStatus,122);
}
lv_obj_t* scrollList(int y,int height){
  auto* list=box(body,24,y,432,height,BG,0);resultList=list;lv_obj_set_style_border_width(list,0,0);
  lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(list,LV_DIR_VER);lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_AUTO);return list;
}
void wifiPage(){
  displayedApCount=radio.apCount;memcpy(displayedAps,radio.aps,sizeof(displayedAps));
  auto* state=box(body,24,12,432,134);
  label(state,radio.connected?"Подключён":"Нет подключения",16,14,398,&boiler_font_25,radio.connected?SKY:TXT);
  auto* ssid=label(state,radio.connected?radio.ssid:radio.hasSaved?"Есть сохранённая сеть":"Выберите сеть",16,54,398,&boiler_font_19,MUTED);
  lv_label_set_long_mode(ssid,LV_LABEL_LONG_DOT);lv_obj_set_height(ssid,28);
  char ip[64];snprintf(ip,sizeof(ip),"%s",radio.connected?radio.ip:"Wi-Fi 2.4 ГГц");label(state,ip,16,96,398,&boiler_font_14,MUTED);
  const bool available=radio.ready&&!radio.busy&&radio.wifiSupported;
  button(body,"Поиск сетей",24,164,208,56,WifiScan,0x245D55,available);
  button(body,"Ввести SSID",248,164,208,56,WifiManual,0x26373B,available);
  auto* list=scrollList(240,280);
  if(!radio.apCount)label(list,radio.busy?"Поиск или подключение...":"Нажмите «Поиск сетей»",16,28,396,&boiler_font_19,MUTED);
  for(unsigned i=0;i<radio.apCount;i++){
    auto* row=button(list,"",0,int(i)*86,422,76,Ap0+i,CARD,available);
    auto* name=label(row,radio.aps[i].ssid,16,11,386,&boiler_font_19);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,26);
    const auto& ap=displayedAps[i];
    const char* security=ap.auth==0?"Открытая сеть":ap.supported?"Защищённая сеть":"Защита не поддерживается";
    char status[108];snprintf(status,sizeof(status),"%s%s",ap.saved?"Сохранена / ":"",security);
    label(row,status,16,45,386,&boiler_font_14,ap.saved?SKY:MUTED);
  }
  button(body,"Другие действия",24,542,432,56,WifiMore);
  if(radio.wifiMessage[0])label(body,radio.wifiMessage,24,618,432,&boiler_font_14,MUTED);
}
void wifiMorePage(){
  button(body,"Отключить Wi-Fi",24,30,432,64,WifiDisconnect,0x26373B,radio.ready&&(radio.connected||radio.autoConnect||(radio.busy&&radio.job==Job::Connect)));
  label(body,"Сохранённые сети",24,128,432,&boiler_font_25);
  auto* list=scrollList(184,432);
  displayedNetworkCount=radio.savedCount;memcpy(displayedNetworks,radio.savedNetworks,sizeof(displayedNetworks));
  if(!displayedNetworkCount)label(list,"Сохранённых сетей пока нет",12,16,398,&boiler_font_19,MUTED);
  const bool available=radio.ready&&!radio.busy;
  for(unsigned i=0;i<displayedNetworkCount;i++){
    const auto& item=displayedNetworks[i];auto* row=box(list,0,int(i)*134,422,124);
    auto* name=label(row,item.ssid,12,6,390,&boiler_font_19);
    lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,28);
    label(row,item.open?"Открытая сеть":"Защищённая сеть",12,39,390,&boiler_font_14,MUTED);
    if(!item.open)button(row,"Новый пароль",12,65,192,48,SavedEdit0+i,0x26373B,available);
    button(row,"Забыть сеть",216,65,192,48,SavedForget0+i,0x26373B,available);
  }
}
void blePage(){
  label(body,"Устройства Bluetooth LE",24,16,432,&boiler_font_19,MUTED);
  button(body,radio.busy?"Поиск...":"Найти устройства",24,66,432,60,BleScan,0x245D55,radio.ready&&radio.bleSupported&&!radio.busy);
  auto* list=scrollList(150,410);
  if(!radio.bleCount)label(list,radio.bleSupported?"Результатов пока нет":"Bluetooth LE недоступен",16,24,396,&boiler_font_19,MUTED);
  for(unsigned i=0;i<radio.bleCount;i++){
    auto* row=box(list,0,int(i)*88,422,78);
    auto* name=label(row,radio.ble[i].name,16,12,386,&boiler_font_19);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,26);
    label(row,radio.ble[i].address,16,48,386,&boiler_font_14,MUTED);
  }
  label(body,"Только поиск, без подключения",24,584,432,&boiler_font_14,MUTED);
  if(radio.bleMessage[0])label(body,radio.bleMessage,24,622,432,&boiler_font_14,MUTED);
}
void displayPage(){
  label(body,"Гасить экран без касаний",24,20,432,&boiler_font_19,MUTED);
  const char* names[]={"Никогда","30 секунд","1 минута","2 минуты","5 минут","10 минут","30 минут"};
  button(body,names[0],24,74,432,60,Timeout0,radio.displayTimeoutSec==0?0x245D55:0x26373B,radio.ready&&!radio.busy);
  for(unsigned i=1;i<7;i++)button(body,names[i],24+224*int((i-1)%2),156+82*int((i-1)/2),208,60,Timeout0+i,radio.displayTimeoutSec==DISPLAY_TIMEOUTS[i]?0x245D55:0x26373B,radio.ready&&!radio.busy&&panel_screen::touchReady());
  label(body,panel_screen::touchReady()?"Первое касание будит экран.\nСледующее — нажимает кнопку.":"Тач недоступен. Сон экрана отключён.",24,442,432,&boiler_font_19,MUTED);
  if(radio.displayMessage[0])label(body,radio.displayMessage,24,552,432,&boiler_font_14,SKY);
}

void haPage(){
  panel_ha::Snapshot ha;panel_ha::snapshot(ha);
  auto* state=box(body,24,16,432,160);
  label(state,"Соединение MQTT",20,22,390,&boiler_font_19,MUTED);
  haStatusLabel=label(state,!ha.ready?"Недоступен":!ha.enabled?"Выключено":ha.connected?"Подключено":"Нет связи",20,74,390,&boiler_font_25);
  label(body,"Настройка — в веб-панели",24,228,432,&boiler_font_25);
  char url[64];snprintf(url,sizeof(url),"%s%s%s",radio.ip[0]?"http://":"",radio.ip[0]?radio.ip:"Сначала подключите Wi-Fi",radio.ip[0]?"/":"");
  label(body,url,24,286,432,&boiler_font_19,SKY);
  label(body,"Вкладка Home Assistant",24,330,432,&boiler_font_19,MUTED);
  label(body,"Передаются только показания.\nКоманд управления отоплением нет.",24,442,432,&boiler_font_19,MUTED);
}

void capacityString(uint64_t bytes,char out[24]){
  const double gb=double(bytes)/1073741824.0;
  const double mb=double(bytes)/1048576.0;
  if(bytes>=1073741824ULL)snprintf(out,24,"%.2f ГБ",gb);
  else if(bytes>=1048576ULL)snprintf(out,24,"%.1f МБ",mb);
  else snprintf(out,24,"%llu Б",(unsigned long long)bytes);
}
void updateStorageLabel(){
  if(!sdStatusLabel)return;
  panel_storage::Snapshot data;
  if(!panel_storage::snapshot(data)){lv_label_set_text(sdStatusLabel,"Ожидание карты");return;}
  char out[260];
  if(!data.mounted)snprintf(out,sizeof(out),"Карта не смонтирована\nПроверьте носитель и FAT32.");
  else{
    char used[24],free[24];capacityString(data.usedBytes,used);capacityString(data.freeBytes,free);
    snprintf(out,sizeof(out),"Свободно: %s\nЗанято: %s\nЖурналирование: %s",free,used,data.logging?"включено":"выключено");
  }
  lv_label_set_text(sdStatusLabel,out);
  if(sdFormatNotice){
    const bool awaiting=data.formatPhase==panel_storage::FormatPhase::AwaitingPhysical;
    char warning[220];
    if(awaiting)snprintf(warning,sizeof(warning),"Стереть ВСЕ файлы на microSD?\nЗапрос из браузера. Осталось %lu с.",(unsigned long)data.formatRemainingSeconds);
    else snprintf(warning,sizeof(warning),"%s",data.formatMessage[0]?data.formatMessage:"Форматирование запрашивается\nиз веб-панели.");
    lv_label_set_text(sdFormatNotice,warning);
    lv_obj_set_style_text_color(sdFormatNotice,c(awaiting?AMBER:MUTED),0);
    for(auto* b:{sdFormatAccept,sdFormatReject})if(b){if(awaiting)lv_obj_clear_flag(b,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(b,LV_OBJ_FLAG_HIDDEN);}
  }
}
void service(){
  menuRow(body,"Обновление","Новые версии панели",16,UpdateMenu,122);
  menuRow(body,"microSD","Место и журналирование",159,StorageMenu,122);
  menuRow(body,"Диагностика","Технические сведения",302,DiagnosticsMenu,122);
  menuRow(body,"О панели","Версия и модель",445,AboutMenu,122);
}
const char* updatePhaseText(){
  if(!updateState.started)return "Запуск обновления";
  if(!updateState.supported)return "Обновление недоступно";
  if(updateState.bootPending)return "Проверка запуска";
  using panel_update::Phase;
  switch(updateState.phase){
    case Phase::Checking:return "Проверка обновлений";
    case Phase::Current:return "Версия актуальна";
    case Phase::Available:return "Доступна новая версия";
    case Phase::Downloading:return "Загрузка";
    case Phase::Verifying:return "Проверка файла";
    case Phase::Installing:return "Установка";
    case Phase::Rebooting:return "Перезапуск панели";
    case Phase::Success:return "Обновление завершено";
    case Phase::Error:return "Обновление не завершено";
    case Phase::Cancelled:return "Обновление отменено";
    case Phase::RolledBack:return "Возврат завершён";
    default:return "Проверить новую версию";
  }
}
void updateLabelText(lv_obj_t* object,const char* text){
  if(strcmp(lv_label_get_text(object),text))lv_label_set_text(object,text);
}
void updateVisibility(lv_obj_t* object,bool visible){
  if(visible==lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN)){
    if(visible)lv_obj_clear_flag(object,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);
  }
}
void updateButtonState(lv_obj_t* object,bool enabled,bool visible){
  if(!object)return;
  updateVisibility(object,visible);
  if(enabled)lv_obj_clear_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);
  const lv_opa_t opacity=enabled?LV_OPA_COVER:LV_OPA_40;
  if(lv_obj_get_style_opa(object,0)!=opacity)lv_obj_set_style_opa(object,opacity,0);
}
void updateUpdateLabels(){
  bool messageChanged=false;
  panel_update::Snapshot next;if(panel_update::snapshot(next)){
    messageChanged=next.supported!=updateState.supported||strcmp(next.message,updateState.message);
    updateState=next;
  }
  if(page==0&&bool(homeUpdateNotice)!=homeUpdateVisible())rebuild=true;
  char out[180];
  if(homeUpdateText){
    if(updateState.bootPending)snprintf(out,sizeof(out),"Проверка новой версии после запуска >");
    else if(updateState.busy){
      if(updateState.total)snprintf(out,sizeof(out),"%s: %u%% >",updatePhaseText(),unsigned(updateState.percent));
      else snprintf(out,sizeof(out),"%s >",updatePhaseText());
    }
    else if(updateState.phase==panel_update::Phase::Success)snprintf(out,sizeof(out),"Установлена %s >",VERSION);
    else if(updateState.phase==panel_update::Phase::RolledBack)snprintf(out,sizeof(out),"Возврат к %s завершён >",VERSION);
    else snprintf(out,sizeof(out),"Доступно обновление %s >",updateState.latestVersion);
    updateLabelText(homeUpdateText,out);
  }
  if(!updateStatus)return;
  updateLabelText(updateLatest,updateState.latestVersion[0]?updateState.latestVersion:"Ещё не проверено");
  updateLabelText(updateStatus,updatePhaseText());
  const lv_color_t ink=c(updateState.phase==panel_update::Phase::Error?AMBER:TXT);
  if(lv_obj_get_style_text_color(updateStatus,0).full!=ink.full)lv_obj_set_style_text_color(updateStatus,ink,0);
  // LONG_DOT changes the label's stored text; compare the original snapshot instead.
  if(messageChanged||!lv_label_get_text(updateMessage)[0])lv_label_set_text(updateMessage,updateState.message[0]?updateState.message:
    updateState.supported?"Проверка новых версий выполняется\nпри подключении к интернету.":"Проверка станет доступна после\nзапуска служб панели.");
  const unsigned percent=updateState.percent>100?100:updateState.percent;
  if(updateState.total){
    char done[24],total[24];capacityString(updateState.done,done);capacityString(updateState.total,total);
    snprintf(out,sizeof(out),"%u%%   %s / %s",percent,done,total);
  }else out[0]=0;
  updateLabelText(updateProgress,out);
  lv_obj_set_width(updateProgressFill,percent?int(428U*percent/100U):1);
  updateVisibility(updateProgressFill,percent!=0);
  if(updateState.errorCode)snprintf(out,sizeof(out),"Код ошибки: %ld",(long)updateState.errorCode);else out[0]=0;
  updateLabelText(updateError,out);
  const bool ready=updateState.started&&updateState.supported&&!updateState.busy&&!updateState.bootPending;
  updateButtonState(updateCheckButton,ready);
  updateButtonState(updateInstallButton,ready&&updateState.available&&updateState.latestVersion[0]);
  updateButtonState(updateCancelButton,updateState.busy&&updateState.cancelAllowed,updateState.busy);
  updateButtonState(updateRollbackButton,ready&&updateState.rollbackAvailable,updateState.rollbackAvailable);
}
void updatePage(){
  auto* versions=box(body,24,16,432,150);
  label(versions,"Установлено",20,12,392,&boiler_font_14,MUTED);
  label(versions,VERSION,20,37,392,&boiler_font_25);
  label(versions,"Последняя версия",20,83,392,&boiler_font_14,MUTED);
  updateLatest=label(versions,"",20,108,392,&boiler_font_19,SKY);
  updateStatus=label(body,"",24,188,432,&boiler_font_25);
  updateMessage=label(body,"",24,232,432,&boiler_font_19,MUTED);
  lv_label_set_long_mode(updateMessage,LV_LABEL_LONG_DOT);lv_obj_set_height(updateMessage,102);
  auto* progress=box(body,24,348,432,10,LINE,5);
  updateProgressFill=box(progress,2,2,1,6,SKY,3);lv_obj_set_style_border_width(updateProgressFill,0,0);
  updateProgress=label(body,"",24,372,432,&boiler_font_14,MUTED);
  updateError=label(body,"",24,406,432,&boiler_font_14,AMBER);
  updateCheckButton=button(body,"Проверить",24,444,208,56,UpdateCheck);
  updateInstallButton=button(body,"Установить",248,444,208,56,UpdateInstall,0x245D55);
  updateCancelButton=button(body,"Отменить обновление",24,520,432,56,UpdateCancel);
  updateRollbackButton=button(body,"Вернуть предыдущую версию",24,592,432,56,UpdateRollback);
  updateUpdateLabels();
}
void storagePage(){
  auto* card=box(body,24,16,432,230);
  label(card,"Хранилище",20,22,390,&boiler_font_25);
  sdStatusLabel=label(card,"Проверка карты...",20,82,390,&boiler_font_19,MUTED);
  sdFormatNotice=label(body,"",24,304,432,&boiler_font_19,MUTED);
  sdFormatReject=button(body,"Отмена",24,466,432,60,SdFormatCancel);
  sdFormatAccept=button(body,"Стереть microSD",24,546,432,60,SdFormatApprove,0x704443);
  updateStorageLabel();
}
void diagnosticsPage(){
  auto* list=scrollList(16,622);char b[200];
  auto* card=box(list,0,0,422,574);
  label(card,"Связь",20,20,380,&boiler_font_19,MUTED);
  snprintf(b,sizeof(b),"Wi-Fi: %s  /  %ld dBm",radio.connected?"подключён":"нет связи",(long)radio.rssi);label(card,b,20,56,380,&boiler_font_19);
  snprintf(b,sizeof(b),"Bluetooth LE: %s",radio.bleSupported?"доступен":"недоступен");label(card,b,20,96,380,&boiler_font_19);
  label(card,"Ресурсы",20,163,380,&boiler_font_19,MUTED);
  heapLabel=label(card,"",20,202,380,&boiler_font_19);
  uptimeLabel=label(card,"",20,242,380,&boiler_font_19);
  label(card,"Радиомодуль",20,309,380,&boiler_font_19,MUTED);
  snprintf(b,sizeof(b),"Host %s  /  C6 %s",radio.hostVersion[0]?radio.hostVersion:"--",radio.c6Version[0]?radio.c6Version:"--");label(card,b,20,348,380,&boiler_font_19);
  label(card,"Запуск дисплея",20,415,380,&boiler_font_19,MUTED);
  snprintf(b,sizeof(b),"Первый кадр: %lu мс\nТач: %s",(unsigned long)panel_screen::boot.firstFrameAt,panel_screen::touchReady()?"готов":"недоступен");label(card,b,20,454,380,&boiler_font_19);
}
void aboutPage(){
  label(body,"Панель котельной",24,36,432,&boiler_font_25);
  label(body,"GUITION JC4880P443",24,98,432,&boiler_font_19,MUTED);
  label(body,VERSION,24,172,432,&boiler_font_55,SKY);
  label(body,"ESP32-P4 + ESP32-C6",24,260,432,&boiler_font_19,MUTED);
  label(body,"Новые версии — в разделе\n«Сервис → Обновление».",24,398,432,&boiler_font_19,MUTED);
}

void updateLive(){
  updateFuel();updateTargetEditor();updateRoomObservation();updateEquipmentReadings();updateUpdateLabels();const uint32_t now=millis();char text[120];
  for(unsigned i=0;i<CHANNELS;i++)if(tempValue[i]){
    const auto& m=measurements[i];temperature(text,sizeof(text),m,now);lv_label_set_text(tempValue[i],text);
    lv_obj_set_style_text_font(tempValue[i],strlen(text)>5?&boiler_font_25:&boiler_font_55,0);
    if(tempState[i]){lv_label_set_text(tempState[i],m.fresh(now)?"":m.count?"Устарело":"Нет данных");lv_obj_set_style_text_color(tempState[i],c(m.count&&!m.fresh(now)?AMBER:MUTED),0);}
  }
  if(heapLabel){snprintf(text,sizeof(text),"Свободно памяти: %lu КБ",(unsigned long)(ESP.getFreeHeap()/1024U));lv_label_set_text(heapLabel,text);}
  if(uptimeLabel){snprintf(text,sizeof(text),"Время работы: %lu мин",(unsigned long)(now/60000U));lv_label_set_text(uptimeLabel,text);}
  if(haStatusLabel){panel_ha::Snapshot ha;panel_ha::snapshot(ha);lv_label_set_text(haStatusLabel,!ha.ready?"Недоступен":!ha.enabled?"Выключено":ha.connected?"Подключено":"Нет связи");}
}

void animX(void* object,int32_t x){lv_obj_set_x(static_cast<lv_obj_t*>(object),x);}
uint32_t signature(){
  uint32_t h=2166136261U;
  const char* strings[]={radio.wifiMessage,radio.bleMessage,radio.timeMessage,radio.ssid,radio.ip};
  for(auto* s:strings)while(*s){h^=uint8_t(*s++);h*=16777619U;}
  h^=uint32_t(radio.ready)|(uint32_t(radio.busy)<<1)|(uint32_t(radio.connected)<<2)|(uint32_t(radio.hasSaved)<<3)|(uint32_t(radio.zone)<<8)|(uint32_t(radio.apCount)<<16)|(uint32_t(radio.bleCount)<<24);
  return h;
}
void render(){
  const lv_coord_t scrollY=resultList && renderedPage==page && renderedView==view?lv_obj_get_scroll_y(resultList):0;
  if(body)lv_obj_del(body);
  haStatusLabel=sdStatusLabel=sdFormatNotice=sdFormatAccept=sdFormatReject=nullptr;
  updateLatest=updateStatus=updateMessage=updateProgress=updateProgressFill=updateError=nullptr;
  updateCheckButton=updateInstallButton=updateCancelButton=updateRollbackButton=nullptr;
  homeUpdateNotice=homeUpdateText=nullptr;
  boilerOnLabel=boilerOffLabel=waterReading=dhwRequests=nullptr;
  for(auto& value:autoValues)value=nullptr;
  for(auto& value:electricValues)value=nullptr;
  for(auto& value:loopRooms)value=nullptr;
  for(auto& value:loopRequests)value=nullptr;
  for(auto& pin:heaterPinLabels)pin=nullptr;
  fuelCardLabels={};fuelCanister={};
  for(auto& icon:tempIcon)icon=nullptr;
  for(unsigned i=0;i<CHANNELS;i++)tempValue[i]=tempState[i]=tempTrend[i]=freshBar[i]=nullptr;
  sensorSummary=sensorHint=liveTime=activity=activityDot=resultList=heapLabel=uptimeLabel=nullptr;
  body=box(lv_scr_act(),0,100,480,673,BG,0);lv_obj_set_style_border_width(body,0,0);
  makeHeader();makeFooter();
  switch(page){
    case 0:home();break;
    case 1:if(view==6||view==7)heatingLoopsPage();else if(view==5)heatingPage();else if(view>=1&&view<=4)equipmentPage();else control();break;
    case 2:fuelPage();break;
    default:
      switch(view){
        case WifiView:wifiPage();break;case BleView:blePage();break;case DisplayView:displayPage();break;
        case ConnectionsView:connectionsPage();break;case HaView:haPage();break;
        case ServiceView:service();break;case StorageView:storagePage();break;case DiagnosticsView:diagnosticsPage();break;
        case AboutView:aboutPage();break;case WifiMoreView:wifiMorePage();break;case UpdateView:updatePage();break;default:settings();break;
      }break;
  }

  if(resultList){lv_obj_update_layout(resultList);lv_obj_scroll_to_y(resultList,scrollY,LV_ANIM_OFF);}
  renderedPage=page;renderedView=view;updateLive();
  if(slideDirection){lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,body);lv_anim_set_exec_cb(&a,animX);lv_anim_set_values(&a,slideDirection>0?480:-480,0);lv_anim_set_time(&a,160);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);slideDirection=0;}
  renderedRevision=radio.revision;renderedSignature=signature();
}
void gesture(lv_event_t*){
  if(overlay || pendingPage>=0)return;
  auto* input=lv_indev_get_act();if(!input)return;
  const lv_dir_t dir=lv_indev_get_gesture_dir(input);if(dir!=LV_DIR_LEFT && dir!=LV_DIR_RIGHT)return;
  if(uint32_t(millis()-lastGesture)<220)return;
  lastGesture=millis();slideDirection=dir==LV_DIR_LEFT?1:-1;pendingPage=adjacent(page,slideDirection);view=0;
  lv_indev_wait_release(input); // prevents a swipe from also clicking a button.
}
void refresh(lv_timer_t*){
  panel_update::uiHeartbeat(firstFrameShown&&panel_screen::touchReady()&&startupServicesReady);
  updateUpdateLabels();
  if(closeDialog)dismiss();
  panel_config::State liveConfig;
  const bool pinsAvailable=panel_config::snapshot(liveConfig);
  if(pinsAvailable && liveConfig.revision!=lastSensorConfigRevision){
    panel_heating_runtime::resetSensors();
    lastSensorConfigRevision=liveConfig.revision;
    for(auto& value:measurements)value=Measurement{};
    fuelLevel.clear();
    // Drop queued measurements from previous hardware assignments.
  }
  panel_sensors::Event sensorEvent;
  for(unsigned n=0;n<18&&panel_sensors::poll(sensorEvent);n++){
    if(sensorEvent.revision!=lastSensorConfigRevision)continue;
    const int temperatureChannel=panel_config::temperatureIndex(sensorEvent.channel);
    if(temperatureChannel>=0)
      measurements[temperatureChannel].put(sensorEvent.temperature,sensorEvent.atMs);
    else if(sensorEvent.channel==uint8_t(panel_config::Channel::FuelLower))
      fuelLevel.lower.put(sensorEvent.reedReached,sensorEvent.atMs);
    else if(sensorEvent.channel==uint8_t(panel_config::Channel::FuelUpper))
      fuelLevel.upper.put(sensorEvent.reedReached,sensorEvent.atMs);
  }
  const bool wasBusy=radio.busy;panel_radio::snapshot(radio);
  if(radio.ready && radio.wifiSupported){
    if(!wifiLogSeen || wifiLogWasOnline!=radio.connected){
      wifiLogSeen=true;wifiLogWasOnline=radio.connected;
      panel_storage::event("wifi",radio.connected?"Connected and IP assigned":"Disconnected");
    }
  }
  if(radio.busy && !wasBusy)activityStart=millis();
  if(radio.ready && (!displaySettingsLoaded || appliedDisplayTimeout!=radio.displayTimeoutSec)){
    displaySettingsLoaded=true;appliedDisplayTimeout=radio.displayTimeoutSec;
    panel_screen::setTimeout(panel_screen::touchReady()?appliedDisplayTimeout:0);
  }
  panel_screen::tick();
  if(pendingPage>=0){page=pendingPage;pendingPage=-1;rebuild=true;}
  // RSSI revisions must not recreate the OTA page or restart its button transitions.
  // Its labels and controls follow the update snapshot above, independently of radio.
  if(!overlay && page==PAGE_SETTINGS && view!=UpdateView && (signature()!=renderedSignature || radio.revision!=renderedRevision))rebuild=true;
  bool touching=false;
  for(auto* input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))
    if(lv_indev_get_type(input)==LV_INDEV_TYPE_POINTER && input->proc.state==LV_INDEV_STATE_PRESSED)touching=true;
  char displayCode[7];uint32_t remaining=0,generation=0;
  const bool pairingVisible=panel_web::pairingCode(displayCode,remaining,&generation);
  if(pairingVisible){
    // Only refreshes backlight idle time; never writes settings/NVS or sends a command.
    panel_screen::setTimeout(panel_screen::timeout());
    if(generation!=shownPairingGeneration&&!touching){
      dismiss();page=0;view=Root;pendingPage=-1;slideDirection=0;shownPairingGeneration=generation;rebuild=true;
    }
  }
  if(rebuild && !overlay && !touching){rebuild=false;render();}
  if(pairBadge&&pairDigits&&headerTitle){
    if(pairingVisible){
      lv_label_set_text(pairDigits,displayCode);lv_obj_clear_flag(pairBadge,LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_width(headerTitle,164);
    }else{
      lv_label_set_text(pairDigits,"");lv_obj_add_flag(pairBadge,LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_width(headerTitle,315);
    }
  }
  memset(displayCode,0,sizeof(displayCode));
  if(activityDot)lv_obj_set_x(activityDot,6+int((millis()/20U)%400U));
  if(uint32_t(millis()-lastTick)>=1000U){
    lastTick=millis();
    panel_heating_runtime::tick(pinsAvailable,lastSensorConfigRevision);
    updateClock();updateLive();updateStorageLabel();
    panel_storage::measurements(measurements,millis());
    panel_web::publish(measurements,fuelLevel,millis());
    panel_ha::publish(measurements,fuelLevel,millis());


  }
}
} // namespace
void setup(){
  // v0.3.1 startup order: first frame precedes GT911 probing, NVS and radio.
  panel_screen::boot.setupAt=millis();uint32_t start=millis();
  if(board_p4_display_init()!=ESP_OK)return;
  panel_screen::boot.displayMs=millis()-start;board_p4_backlight(true);start=millis();
  if(!lvgl_glue_start(true))return;
  panel_screen::boot.lvglMs=millis()-start;
  if(!lvgl_glue_lock(0))return;
  auto* s=lv_scr_act();lv_obj_clean(s);lv_obj_clear_flag(s,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(s,c(BG),0);
  lv_obj_set_style_pad_all(s,0,0);lv_obj_add_event_cb(s,gesture,LV_EVENT_GESTURE,nullptr);render();
  lv_obj_invalidate(s);lv_refr_now(nullptr);board_p4_backlight(true);panel_screen::boot.firstFrameAt=millis();firstFrameShown=true;
  auto* timer=lv_timer_create(refresh,100,nullptr);lvgl_glue_unlock();if(!timer)return;
  // Existing 0.3.1 glue returns RELEASED until begin publishes touchReady.
  start=millis();const bool touchOK=board_p4_touch_init()==ESP_OK && board_p4_get_touch_handle()!=nullptr;
  const uint32_t touchMs=millis()-start;
  if(lvgl_glue_lock(0)){panel_screen::boot.touchMs=touchMs;panel_screen::begin(touchOK);lvgl_glue_unlock();}
  Serial.begin(115200);
  const bool configReady=panel_config::begin(),sensorsReady=panel_sensors::begin(),radioReady=panel_radio::begin();
  panel_boiler::begin(); // Optional local preferences. A read fault disables editing, not the rest of the panel.
  panel_targets::begin();
  panel_heating_settings::begin();
  const bool storageReady=panel_storage::begin(),haReady=panel_ha::begin(),webReady=panel_web::begin();
  const bool updateReady=panel_update::begin();
  if(lvgl_glue_lock(0)){
    startupServicesReady=configReady&&sensorsReady&&radioReady&&storageReady&&haReady&&webReady&&updateReady;
    lvgl_glue_unlock();
  }
  char timing[180];snprintf(timing,sizeof(timing),"[BoilerPanel] %s equipment=NOT_CONNECTED outputs=NONE display=%lu ms LVGL=%lu ms first-frame=%lu ms touch=%lu ms",VERSION,(unsigned long)panel_screen::boot.displayMs,(unsigned long)panel_screen::boot.lvglMs,(unsigned long)panel_screen::boot.firstFrameAt,(unsigned long)touchMs);Serial.println(timing);
  panel_storage::event("boot","Panel application started");
}
void loop(){delay(20);}
