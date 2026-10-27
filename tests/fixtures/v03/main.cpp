/* Boiler Panel v0.3.0 — independent JC4880P443 panel, LVGL 8.3.
 * No assigned sensor hardware, actuator outputs, or borrowed heater logic.
 * Uses the existing, working portrait display/touch adapter and fonts.
 */
#include <Arduino.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "board_p4.h"
#include "lvgl_glue.h"
#include "panel_radio.h"
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
constexpr uint32_t BG=0x09121F, CARD=0x152439, LINE=0x293D54, TXT=0xF1F6FC;
constexpr uint32_t MUTED=0x96ABC1, SKY=0x62CFFF, GREEN=0x66DDB4, AMBER=0xF4C083;
const char* const PAGES[]={"Главная","Управление","Настройки","Сервис"};
const char* const SENSOR_NAMES[]={"В котельной","На улице","Теплоноситель","Топливо"};
const uint32_t ACCENT[]={0x6BDDB5,0x72BDF8,0xF3B876,0xB9A4EF};
Measurement measurements[CHANNELS]; // Invalid until a future, explicit sensor integration.
RadioState radio;
int page=0, pendingPage=-1, slideDirection=0, view=0;
uint32_t clicks=0, lastGesture=0, lastTick=0, renderedRevision=0, renderedSignature=0;
bool rebuild=false, closeDialog=false;
lv_obj_t *header=nullptr,*body=nullptr,*clockLabel=nullptr,*dateLabel=nullptr,*footer=nullptr;
lv_obj_t *overlay=nullptr,*dialogError=nullptr,*ssidField=nullptr,*passwordField=nullptr,*timeField=nullptr,*activeField=nullptr,*keyboard=nullptr;
int dialogKind=0; // 0 none, 1 Wi-Fi credentials, 2 manual clock, 3 information.
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
enum Action:uintptr_t{Home=1,Back,Wifi,Ble,Time,WifiScan,WifiManual,WifiDisconnect,WifiForget,BleScan,ManualClock,ClockSave,Connect,Cancel,ToggleOpen,TogglePassword,ServiceRefresh,Zone0,Zone1,Zone2,Temp0=40,Temp1,Temp2,Temp3,Ap0=100};
void action(lv_event_t* e);
lv_obj_t* button(lv_obj_t* parent,const char* title,int x,int y,int w,int h,uintptr_t code,uint32_t fill=0x214258,bool enabled=true){
  auto* o=lv_btn_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
  lv_obj_set_style_bg_color(o,c(fill),0);lv_obj_set_style_bg_color(o,c(0x365A73),LV_STATE_PRESSED);
  lv_obj_set_style_radius(o,12,0);lv_obj_set_style_shadow_width(o,0,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_pad_all(o,3,0);
  lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(o,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(code));
  auto* t=label(o,title,0,0,w-12,&boiler_font_14);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(t);
  if(!enabled){lv_obj_add_state(o,LV_STATE_DISABLED);lv_obj_set_style_opa(o,LV_OPA_40,0);}return o;
}
void clearSecretFields(){
  if(passwordField)lv_textarea_set_text(passwordField,"");
  ssidField=passwordField=timeField=activeField=keyboard=dialogError=nullptr;
  memset(selectedSsid,0,sizeof(selectedSsid));
}
void dismiss(){if(overlay){clearSecretFields();lv_obj_del(overlay);}overlay=nullptr;dialogKind=0;closeDialog=false;}
void error(const char* msg){if(dialogError)lv_label_set_text(dialogError,msg);}
bool send(Command& command){
  const bool ok=panel_radio::submit(command);
  // Erase the command copy after the queue has copied it.
  volatile char* p=command.password;for(size_t i=0;i<sizeof(command.password);i++)p[i]=0;
  if(!ok)error("Операция уже выполняется. Повторите после завершения");
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
void keyboardEvent(lv_event_t* e){
  if(!activeField || lv_obj_has_state(activeField,LV_STATE_DISABLED))return;
  const uint16_t id=lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
  if(id==LV_BTNMATRIX_BTN_NONE)return;
  const char* t=lv_btnmatrix_get_btn_text(lv_event_get_target(e),id);if(!t)return;
  if(!strcmp(t,"ABC")){upper=true;symbols=false;lv_btnmatrix_set_map(keyboard,UPPER);}
  else if(!strcmp(t,"abc")){upper=false;symbols=false;lv_btnmatrix_set_map(keyboard,LOWER);}
  else if(!strcmp(t,"123")){symbols=true;lv_btnmatrix_set_map(keyboard,SYMBOLS);}
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
  lv_btnmatrix_set_map(keyboard,LOWER);upper=false;symbols=false;
  lv_obj_clear_flag(keyboard,LV_OBJ_FLAG_GESTURE_BUBBLE);lv_obj_add_event_cb(keyboard,keyboardEvent,LV_EVENT_VALUE_CHANGED,nullptr);
}
void credentials(const AccessPoint* ap){
  dismiss();dialogKind=1;ssidLocked=ap!=nullptr;openNetwork=ap && ap->auth==0;
  overlay=box(lv_layer_top(),0,0,480,800,BG,0);
  label(overlay,"Подключение Wi-Fi",18,23,300,&boiler_font_25);button(overlay,"Домой",350,14,112,50,Home);
  if(ap){
    snprintf(selectedSsid,sizeof(selectedSsid),"%s",ap->ssid);
    label(overlay,"Выбранная сеть",18,88,442,&boiler_font_14,MUTED);label(overlay,ap->ssid,18,116,442,&boiler_font_19);
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
void action(lv_event_t* e){
  ++clicks;const uintptr_t code=reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  Command cmd;
  if(code>=Ap0 && code<Ap0+MAX_AP){
    const unsigned i=unsigned(code-Ap0);if(i<radio.apCount){if(radio.aps[i].supported)credentials(&radio.aps[i]);else info("Тип защиты","Эта версия поддерживает открытые сети и WPA2/WPA3 Personal. Корпоративные сети и WEP не поддерживаются.");}return;
  }
  if(code>=Temp0 && code<=Temp3){
    const unsigned i=unsigned(code-Temp0);char b[300];
    if(!measurements[i].valid)snprintf(b,sizeof(b),"Датчик не назначен.\n\nРеальных измерений и истории пока нет. Тип датчика и подключение зададим отдельно для этой котельной.");
    else snprintf(b,sizeof(b),"Температура: %.1f°C\nМинимум: %.1f°C\nМаксимум: %.1f°C\nПоследнее обновление: %lu с назад",measurements[i].value,measurements[i].minimum,measurements[i].maximum,(unsigned long)((millis()-measurements[i].updated)/1000U));
    info(SENSOR_NAMES[i],b);return;
  }
  switch(code){
    case Home:closeDialog=true;pendingPage=0;slideDirection=-1;view=0;break;
    case Back:view=0;rebuild=true;break;
    case Wifi:view=1;rebuild=true;break;
    case Ble:view=2;rebuild=true;break;
    case Time:view=3;rebuild=true;break;
    case WifiScan:cmd.job=Job::WifiScan;send(cmd);break;
    case WifiManual:credentials(nullptr);break;
    case WifiDisconnect:cmd.job=Job::Disconnect;send(cmd);break;
    case WifiForget:cmd.job=Job::Forget;send(cmd);break;
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
    default:break;
  }
}
void updateClock(){
  if(!clockLabel)return;
  const time_t now=time(nullptr);char t[16]="--:--",d[48]="Нет времени";
  if(now>=time_t(1704067200)){tm local={};localtime_r(&now,&local);strftime(t,sizeof(t),"%H:%M",&local);strftime(d,sizeof(d),"%d.%m.%Y",&local);}
  lv_label_set_text(clockLabel,t);lv_label_set_text(dateLabel,d);
}
void makeHeader(){
  if(header)lv_obj_del(header);
  clockLabel=dateLabel=nullptr;
  header=box(lv_scr_act(),0,0,480,100,BG,0);lv_obj_set_style_border_width(header,0,0);
  label(header,page==0?"КОТЕЛЬНАЯ":PAGES[page],18,21,310,&boiler_font_25);
  label(header,page==0?"Четыре температурных канала":"Самостоятельная панель",18,62,310,&boiler_font_14,MUTED);
  if(page==0){
    clockLabel=label(header,"--:--",337,18,125,&boiler_font_25,SKY);lv_obj_set_style_text_align(clockLabel,LV_TEXT_ALIGN_RIGHT,0);
    dateLabel=label(header,"Нет времени",326,59,136,&boiler_font_14,MUTED);lv_obj_set_style_text_align(dateLabel,LV_TEXT_ALIGN_RIGHT,0);updateClock();
  }else button(header,"Домой",350,20,112,52,Home,0x1D354A);
}
void makeFooter(){
  if(footer)lv_obj_del(footer);
  footer=box(lv_scr_act(),0,773,480,27,BG,0);lv_obj_set_style_border_width(footer,0,0);
  // Passive page indicators, not buttons.
  for(int i=0;i<4;i++){auto* dot=box(footer,206+i*19,9,i==page?16:7,6,i==page?SKY:LINE,3);lv_obj_set_style_border_width(dot,0,0);lv_obj_clear_flag(dot,LV_OBJ_FLAG_CLICKABLE);}
}
void home(){
  for(unsigned i=0;i<CHANNELS;i++){
    const int x=16+(i%2)*232,y=22+(i/2)*251;
    auto* tile=box(body,x,y,216,233);lv_obj_set_style_bg_grad_color(tile,c(0x102033),0);lv_obj_set_style_bg_grad_dir(tile,LV_GRAD_DIR_VER,0);
    lv_obj_add_event_cb(tile,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(Temp0+i)));
    lv_obj_set_style_border_color(tile,c(ACCENT[i]),LV_STATE_PRESSED);
    auto* mark=box(tile,16,16,30,5,ACCENT[i],3);lv_obj_set_style_border_width(mark,0,0);lv_obj_clear_flag(mark,LV_OBJ_FLAG_CLICKABLE);
    label(tile,SENSOR_NAMES[i],15,34,186,&boiler_font_19);
    char value[24];temperature(value,sizeof(value),measurements[i],millis());
    label(tile,value,13,80,185,strlen(value)>5?&boiler_font_25:&boiler_font_55);label(tile,"°C",166,146,35,&boiler_font_25,ACCENT[i]);
    label(tile,measurements[i].fresh(millis())?"Измерение получено":"Нет данных",15,168,182,&boiler_font_14,MUTED);
    label(tile,"Нажмите для деталей",15,199,185,&boiler_font_14,ACCENT[i]);
  }
  auto* status=box(body,16,538,448,78,0x11243A);label(status,"Датчики ещё не назначены",15,12,413,&boiler_font_19);
  label(status,radio.connected?"Wi-Fi подключён. Измерения не поступают":"Показания появятся после подключения датчиков",15,45,413,&boiler_font_14,MUTED);
  label(body,"Свайп влево / вправо - смена страницы",18,640,442,&boiler_font_14,MUTED);
}
void control(){
  auto* p=box(body,16,24,448,210);label(p,"Оборудование не добавлено",18,20,410,&boiler_font_25);
  label(p,"Здесь появится управление устройствами\nименно этой котельной.\n\nРеальные выходы пока не назначены.",18,76,410,&boiler_font_19,MUTED);
  auto* q=box(body,16,254,448,154);label(q,"Без переноса старых проектов",18,20,410,&boiler_font_19,SKY);
  label(q,"Нет прежних котлов, насосов, контроллеров\nи тестовой автоматики. Подключение\nоборудования выполняется отдельно.",18,61,410,&boiler_font_14,MUTED);
  label(body,"Свайп влево - настройки панели",18,640,442,&boiler_font_14,MUTED);
}
void settings(){
  auto* w=box(body,16,22,448,145);label(w,"Wi-Fi",18,16,410,&boiler_font_25,SKY);
  label(w,radio.connected?"Подключено к сети":"Выбор сети и подключение",18,55,410,&boiler_font_14,MUTED);
  button(w,"Открыть Wi-Fi",18,86,410,44,Wifi);
  auto* b=box(body,16,183,448,145);label(b,"Bluetooth рядом",18,16,410,&boiler_font_25,0xB9A4EF);
  label(b,"Поиск BLE-устройств, без сопряжения",18,55,410,&boiler_font_14,MUTED);button(b,"Открыть Bluetooth",18,86,410,44,Ble);
  auto* t=box(body,16,344,448,145);label(t,"Дата и время",18,16,410,&boiler_font_25,GREEN);
  label(t,"Синхронизация NTP и часовой пояс",18,55,410,&boiler_font_14,MUTED);button(t,"Настроить время",18,86,410,44,Time);
  label(body,"Сети и устройства отображаются только\nпо результатам настоящего поиска.",18,525,442,&boiler_font_19,MUTED);
  label(body,"Свайп влево / вправо - смена страницы",18,640,442,&boiler_font_14,MUTED);
}
void subsection(const char* title){button(body,"Назад",16,10,105,44,Back);label(body,title,139,16,323,&boiler_font_25);}
lv_obj_t* scrollList(int y,int height){
  auto* list=box(body,16,y,448,height,BG);lv_obj_set_style_border_width(list,0,0);
  lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(list,LV_DIR_VER);lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_AUTO);
  // Horizontal page gestures are available from the header and non-scrolling areas.
  return list;
}
void wifiPage(){
  subsection("Wi-Fi");
  auto* p=box(body,16,69,448,141);label(p,radio.connected?"Подключено":"Не подключено",14,12,415,&boiler_font_19,radio.connected?GREEN:MUTED);
  char b[128];if(radio.connected)snprintf(b,sizeof(b),"%s\nIP: %s   %ld dBm",radio.ssid,radio.ip,(long)radio.rssi);else snprintf(b,sizeof(b),"Сети 2.4 ГГц. DHCP.\n%s",radio.hasSaved?"Есть сохранённая сеть":"Сохранённой сети нет");
  label(p,b,14,43,415,&boiler_font_14,MUTED);label(p,radio.wifiMessage,14,91,415,&boiler_font_14,SKY);
  const bool available=radio.ready && !radio.busy && radio.wifiSupported;
  button(body,radio.busy?"Выполняется...":"Найти сети",16,222,216,46,WifiScan,0x157A92,available);
  button(body,"Ввести SSID",248,222,216,46,WifiManual,0x214258,available);
  button(body,"Отключить",16,280,216,44,WifiDisconnect,0x214258,available && radio.connected);
  button(body,"Забыть сеть",248,280,216,44,WifiForget,0x643B48,available && radio.hasSaved);
  auto* list=scrollList(342,290);
  if(!radio.apCount)label(list,radio.busy?"Идёт поиск или подключение...":"Нажмите Найти сети.\nЗдесь нет демонстрационных сетей.",10,15,420,&boiler_font_19,MUTED);
  for(unsigned i=0;i<radio.apCount;i++){
    auto* row=button(list,"",0,int(i)*78,438,69,Ap0+i,0x192D42,available);
    auto* name=label(row,radio.aps[i].ssid,12,7,408,&boiler_font_19);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,25);
    snprintf(b,sizeof(b),"%ld dBm   %s",(long)radio.aps[i].rssi,radio.aps[i].auth==0?"Открытая сеть":radio.aps[i].supported?"Защищённая сеть":"Тип защиты не поддерживается");
    label(row,b,12,38,408,&boiler_font_14,MUTED);
  }
  label(body,"Свайп по заголовку - переход на другую страницу",18,646,442,&boiler_font_14,MUTED);
}
void blePage(){
  subsection("Bluetooth LE");
  auto* p=box(body,16,69,448,143);label(p,"Устройства поблизости",14,12,415,&boiler_font_19,0xB9A4EF);
  label(p,"Показываются устройства, передающие BLE-объявления.\nBluetooth Classic и автоматического подключения нет.",14,49,415,&boiler_font_14,MUTED);
  label(p,radio.bleMessage,14,101,415,&boiler_font_14,SKY);
  button(body,radio.busy?"Выполняется...":"Поиск BLE (5 секунд)",16,226,448,49,BleScan,0x3E4166,radio.ready && radio.bleSupported && !radio.busy);
  auto* list=scrollList(291,335);
  if(!radio.bleCount)label(list,radio.bleSupported?"Результатов поиска пока нет":"BLE не включён в текущей сборке.\nСведения о поддержке - в Сервисе.",10,17,420,&boiler_font_19,MUTED);
  for(unsigned i=0;i<radio.bleCount;i++){
    auto* r=box(list,0,int(i)*91,438,82);auto* name=label(r,radio.ble[i].name,12,9,408,&boiler_font_19);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_height(name,25);
    char b[96];snprintf(b,sizeof(b),"%s   %ld dBm",radio.ble[i].address,(long)radio.ble[i].rssi);label(r,b,12,47,408,&boiler_font_14,MUTED);
  }
  label(body,"До 16 устройств. Значение dBm - уровень сигнала",18,645,442,&boiler_font_14,MUTED);
}
void timePage(){
  subsection("Дата и время");
  auto* p=box(body,16,69,448,137);char b[80]="--:--   Время не установлено";time_t now=time(nullptr);
  if(now>=time_t(1704067200)){tm t={};localtime_r(&now,&t);strftime(b,sizeof(b),"%H:%M:%S   %d.%m.%Y",&t);}
  label(p,b,14,20,415,&boiler_font_25,SKY);label(p,"Часы на главной обновляются автоматически.\nNTP включается после подключения Wi-Fi.",14,74,415,&boiler_font_14,MUTED);
  label(body,"Часовой пояс",18,230,442,&boiler_font_19);
  for(unsigned i=0;i<3;i++)button(body,panel_radio::zoneName(i),16,268+i*59,448,48,Zone0+i,radio.zone==i?0x147C90:0x214258,radio.ready && !radio.busy);
  button(body,"Установить дату и время вручную",16,466,448,52,ManualClock,0x214258,radio.ready && !radio.busy);
  label(body,radio.timeMessage,18,538,442,&boiler_font_14,SKY);
  label(body,"Часовой пояс сохраняется. После полного\nотключения питания время снова устанавливается\nпо сети или вручную. Значения не имитируются.",18,589,442,&boiler_font_14,MUTED);
}
void service(){
  auto* p=box(body,16,22,448,345);char b[128];
  label(p,"Панель JC4880P443",16,17,414,&boiler_font_25);
  snprintf(b,sizeof(b),"Версия: %s  /  ESP32-P4",VERSION);label(p,b,16,64,414,&boiler_font_19,SKY);
  snprintf(b,sizeof(b),"Свободно памяти: %lu КБ",(unsigned long)(ESP.getFreeHeap()/1024U));label(p,b,16,106,414);
  snprintf(b,sizeof(b),"Работа: %lu с   Нажатий: %lu",(unsigned long)(millis()/1000U),(unsigned long)clicks);label(p,b,16,140,414);
  snprintf(b,sizeof(b),"Wi-Fi Remote: %s",radio.wifiSupported?"доступен":"нет в сборке");label(p,b,16,178,414);
  snprintf(b,sizeof(b),"Bluetooth LE: %s",radio.bleSupported?"доступен":"нет в сборке");label(p,b,16,212,414);
  snprintf(b,sizeof(b),"ESP-Hosted: %s",radio.hostVersion[0]?radio.hostVersion:"--");label(p,b,16,250,414);
  snprintf(b,sizeof(b),"Прошивка C6: %s",radio.c6Version[0]?radio.c6Version:"не прочитана");label(p,b,16,284,414);
  auto* symbolsBox=box(body,16,384,448,98);label(symbolsBox,"Проверка символов",14,11,416,&boiler_font_19);
  label(symbolsBox,"+ - --.- 12:34 23.5°C",14,49,416,&boiler_font_25,SKY);
  button(body,"Обновить сведения",16,502,448,52,ServiceRefresh);
  label(body,"C6 автоматически не обновляется.\nСписки Wi-Fi и BLE показывают реальные результаты.\nИзмерительных датчиков и выходов пока нет.",18,581,442,&boiler_font_14,MUTED);
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
  if(body)lv_obj_del(body);
  body=box(lv_scr_act(),0,100,480,673,BG,0);lv_obj_set_style_border_width(body,0,0);
  makeHeader();makeFooter();
  switch(page){case 0:home();break;case 1:control();break;case 2:if(view==1)wifiPage();else if(view==2)blePage();else if(view==3)timePage();else settings();break;default:service();break;}
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
  if(closeDialog)dismiss();
  panel_radio::snapshot(radio);
  if(pendingPage>=0){page=pendingPage;pendingPage=-1;rebuild=true;}
  if(!overlay && (page==0 || page==2) && signature()!=renderedSignature)rebuild=true;
  if(rebuild && !overlay){rebuild=false;render();}
  if(uint32_t(millis()-lastTick)>=1000U){lastTick=millis();updateClock();}
}
} // namespace
void setup(){
  Serial.begin(115200);delay(120);
  Serial.println("[BoilerPanel] v0.3.0 / swipe UI / independent panel");
  if(board_p4_display_init()!=ESP_OK){Serial.println("[BoilerPanel] DISPLAY INIT FAILED");return;}
  if(!lvgl_glue_start(true)){Serial.println("[BoilerPanel] LVGL INIT FAILED");return;}
  if(!lvgl_glue_lock(0)){Serial.println("[BoilerPanel] LVGL LOCK FAILED");return;}
  auto* s=lv_scr_act();lv_obj_clean(s);lv_obj_clear_flag(s,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(s,c(BG),0);
  lv_obj_set_style_pad_all(s,0,0);lv_obj_add_event_cb(s,gesture,LV_EVENT_GESTURE,nullptr);render();
  auto* timer=lv_timer_create(refresh,100,nullptr);lvgl_glue_unlock();
  if(!timer){Serial.println("[BoilerPanel] UI TIMER FAILED");return;}
  if(!panel_radio::begin())Serial.println("[BoilerPanel] RADIO TASK NOT CREATED; UI remains available");
  Serial.println("[BoilerPanel] Ready. No sensor/actuator GPIOs assigned. C6 firmware unchanged.");
}
void loop(){delay(100);}
