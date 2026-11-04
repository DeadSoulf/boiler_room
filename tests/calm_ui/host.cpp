#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <time.h>
#include "fakes.h"
#include "panel_screen.h"
#ifndef UI_SOURCE
#define UI_SOURCE "../../src/main.cpp"
#endif
// Fixed clock for repeatable previews, not a value from a real device.
time_t preview_time(time_t* out){tm t={};t.tm_year=126;t.tm_mon=10;t.tm_mday=4;t.tm_hour=12;t.tm_min=40;time_t v=mktime(&t);if(out)*out=v;return v;}
#define time preview_time
#include UI_SOURCE
#undef time
std::vector<lv_color_t> pixels(480*800);
void flush(lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* src){for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)pixels[y*480+x]=*src++;lv_disp_flush_ready(d);}
int mx=10,my=10;bool down=false;
void pointer(lv_indev_drv_t*,lv_indev_data_t* d){d->point.x=mx;d->point.y=my;d->state=panel_screen_touch_ready()&&panel_screen_filter_touch(down)?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
void step(unsigned ms=220){for(unsigned i=0;i<ms;i+=10){fakeMillis+=10;lv_tick_inc(10);lv_timer_handler();}}
void save(const char* name){lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);std::ofstream f(std::string(name)+".ppm",std::ios::binary);f<<"P6\n480 800\n255\n";for(auto p:pixels){auto v=lv_color_to32(p);unsigned char rgb[]={uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)};f.write(reinterpret_cast<char*>(rgb),3);}}
unsigned clipped=0,checks=0;
void bounds(lv_obj_t* obj,bool scrolling=false){
 if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return;
 if(lv_obj_check_type(obj,&lv_label_class)&&!scrolling){auto* parent=lv_obj_get_parent(obj);lv_area_t a,b;lv_obj_get_coords(obj,&a);lv_obj_get_coords(parent,&b);
  if(a.x1<b.x1||a.x2>b.x2||a.y1<b.y1||a.y2>b.y2){++clipped;fprintf(stderr,"CLIPPED [%s] (%d,%d,%d,%d) parent(%d,%d,%d,%d)\n",lv_label_get_text(obj),a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);}}
 for(unsigned i=0;i<lv_obj_get_child_cnt(obj);i++)bounds(lv_obj_get_child(obj,i),scrolling||lv_obj_has_flag(obj,LV_OBJ_FLAG_SCROLLABLE));
}
void check(){++checks;lv_obj_update_layout(lv_scr_act());bounds(lv_scr_act());bounds(lv_layer_top());}
lv_obj_t* find(lv_obj_t* obj,const char* name){if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return nullptr;if(lv_obj_check_type(obj,&lv_label_class)&&!strcmp(lv_label_get_text(obj),name))return obj;for(unsigned i=0;i<lv_obj_get_child_cnt(obj);i++)if(auto* found=find(lv_obj_get_child(obj,i),name))return found;return nullptr;}
void clickAt(int x,int y){mx=x;my=y;down=true;step(80);down=false;step(240);}
void touch(const char* name){auto* n=find(overlay?lv_layer_top():lv_scr_act(),name);if(!n){fprintf(stderr,"MISSING CLICK [%s] page=%d view=%d overlay=%d\n",name,page,view,int(overlay!=nullptr));abort();}auto* o=lv_obj_get_parent(n);assert(!lv_obj_has_state(o,LV_STATE_DISABLED));lv_obj_update_layout(o);lv_area_t a;lv_obj_get_coords(o,&a);assert(a.x1>=0&&a.y1>=0&&a.x2<480&&a.y2<800);int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;lv_point_t p={lv_coord_t(x),lv_coord_t(y)};auto* hit=lv_indev_search_obj(overlay?lv_layer_top():lv_scr_act(),&p);while(hit&&hit!=o)hit=lv_obj_get_parent(hit);assert(hit==o);clickAt(x,y);check();}
void swipe(bool left=true){mx=left?420:60;my=85;down=true;step(60);for(int i=0;i<8;i++){mx+=left?-42:42;step(40);}down=false;step(320);check();}
void go(int p,int v=0){dismiss();page=p;view=v;pendingPage=-1;rebuild=false;render();step();check();}
int main(){
 setenv("TZ","UTC0",1);tzset();lv_init();
 static lv_color_t buf[480*40];static lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,buf,nullptr,480*40);
 lv_disp_drv_t display;lv_disp_drv_init(&display);display.hor_res=480;display.ver_res=800;display.flush_cb=flush;display.draw_buf=&draw;lv_disp_drv_register(&display);
 lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer;lv_indev_drv_register(&input);
 auto& radioFixture=panel_radio::fixture;radioFixture.ready=true;radioFixture.wifiSupported=true;radioFixture.bleSupported=true;radioFixture.connected=true;radioFixture.hasSaved=true;
 strcpy(radioFixture.ssid,"Example-WiFi");strcpy(radioFixture.ip,"192.0.2.10");radioFixture.rssi=-53;strcpy(radioFixture.hostVersion,"2.9.6");strcpy(radioFixture.c6Version,"2.9.6");
 auto& sd=panel_storage::fixture;sd.started=sd.mounted=sd.logging=true;sd.freeBytes=62602390528ULL;sd.usedBytes=6144;
 panel_ha::fixture.ready=panel_ha::fixture.supported=true;
 setup();step(400);check();save("home-no-data");
#ifdef BEFORE_CAPTURE
 for(int p=1;p<4;p++){go(p);save(p==1?"equipment-before":p==2?"settings-before":"service-before");}
 puts("PASS: baseline screenshots from actual old LVGL source, isolated fixtures");return 0;
#else
 assert(!strcmp(PAGES[2],"Топливо")&&!strcmp(PAGES[3],"Настройки"));
 for(unsigned i=0;i<3;i++){assert(tempValue[i]&&strstr(lv_label_get_text(tempValue[i]),"--"));assert(lv_obj_get_style_text_font(tempValue[i],0)==&boiler_font_55);assert(!freshBar[i]&&!tempTrend[i]);}
 assert(!tempValue[3]&&fuelCanister.temperature&&fuelCanister.root);
 assert(lv_obj_get_style_text_font(fuelCanister.temperature,0)==&boiler_font_19);
 assert(lv_obj_get_child_cnt(body)==4&&!find(body,"Уровень топлива"));
 assert(!strcmp(lv_label_get_text(fuelCanister.temperature),"--.- °C"));
 assert(lv_obj_has_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(fuelCanister.glyph),"?"));
 touch("Топливо");assert(page==2&&!overlay);touch("Температура топлива");assert(overlay&&detailChannel==3);save("temperature-no-data");touch("Назад");touch("Домой");assert(!overlay&&page==0);
 swipe();assert(page==1&&view==0);save("equipment");
 touch("Электрокотёл");assert(page==1&&view==2);save("electric");touch("<");assert(page==1&&view==0);
 touch("Дизельный отопитель");assert(view==3);touch("Домой");assert(page==0&&!overlay);
 swipe();swipe();assert(page==2);save("fuel-no-data");
 touch("Температура топлива");assert(overlay&&detailChannel==3);touch("Назад");assert(!overlay&&page==2);
 swipe();assert(page==PAGE_SETTINGS);save("settings");touch("Подключения");assert(view==ConnectionsView);save("connections");
 touch("Wi-Fi");assert(view==WifiView);save("wifi");touch("Другие действия");assert(view==WifiMoreView);
 unsigned sent=panel_radio::submitted;touch("Забыть сеть");assert(overlay&&panel_radio::submitted==sent);save("forget-confirm");touch("Отмена");assert(!overlay&&view==WifiMoreView&&panel_radio::submitted==sent);
 touch("Отключить Wi-Fi");panel_radio::accept=false;touch("Отключить");assert(overlay&&dialogError&&strlen(lv_label_get_text(dialogError))>0);panel_radio::accept=true;touch("Отключить");assert(!overlay&&panel_radio::submitted==sent+1&&panel_radio::last.job==Job::Disconnect);
 touch("<");assert(view==WifiView);touch("<");assert(view==ConnectionsView);
 touch("Home Assistant");assert(view==HaView);save("home-assistant");touch("<");assert(view==ConnectionsView);touch("<");assert(view==Root);
 touch("Веб-доступ");assert(view==WebView);touch("Создать код");step(1100);assert(panel_web::newCodes==1&&strstr(lv_label_get_text(webPairLabel),"123456"));save("pairing-fixture");touch("Отозвать доступ");step(1100);assert(!panel_web::armed);touch("<");
 touch("Сервис");assert(view==ServiceView);save("service");touch("microSD");assert(view==StorageView);assert(lv_obj_has_flag(sdFormatAccept,LV_OBJ_FLAG_HIDDEN));save("storage");
 sd.formatPhase=panel_storage::FormatPhase::AwaitingPhysical;sd.formatRemainingSeconds=80;updateStorageLabel();check();save("storage-confirm-fixture");touch("Отмена");assert(panel_storage::cancelled==1&&panel_storage::accepted==0);
 sd.formatPhase=panel_storage::FormatPhase::AwaitingPhysical;updateStorageLabel();touch("Стереть microSD");assert(panel_storage::accepted==1);assert(lv_obj_has_flag(sdFormatAccept,LV_OBJ_FLAG_HIDDEN));
 touch("<");assert(view==ServiceView);touch("Диагностика");save("diagnostics");touch("<");touch("О панели");save("about");touch("Домой");assert(page==0);
 go(PAGE_SETTINGS,WifiView);radioFixture.apCount=16;
 for(unsigned i=0;i<16;i++){auto& ap=radioFixture.aps[i];memset(ap.ssid,'W',32);ap.ssid[32]=0;ap.supported=true;ap.auth=3;ap.rssi=-50-int(i);}
 ++radioFixture.revision;step(300);check();save("wifi-results-fixture");
 lv_obj_scroll_to_y(resultList,160,LV_ANIM_OFF);step();assert(lv_obj_get_scroll_y(resultList)>0);
 ++radioFixture.revision;step();assert(lv_obj_get_scroll_y(resultList)>0);
 lv_obj_scroll_to_y(resultList,0,LV_ANIM_OFF);step();mx=150;my=375;down=true;step(80);
 strcpy(radioFixture.aps[0].ssid,"Replacement");++radioFixture.revision;step(140);assert(displayedAps[0].ssid[0]=='W');down=false;step(300);assert(overlay&&selectedSsid[0]=='W');check();save("credentials-fixture");dismiss();
 go(0);const float values[]={22.4f,-8.2f,61.7f,34.8f};for(unsigned i=0;i<4;i++)measurements[i].put(values[i],millis());fuelLevel.put(true,false,millis());step(1100);save("home-values-fixture");check();
 for(unsigned i=0;i<3;i++)assert(!strcmp(lv_label_get_text(tempState[i]),""));
 assert(!strcmp(lv_label_get_text(fuelCanister.temperature),"34.8 °C"));
 go(2);save("fuel-values-fixture");go(0);step(31000);assert(!strcmp(lv_label_get_text(tempState[0]),"Устарело"));save("home-stale-fixture");
 // Screen wake consumes the first touch rather than activating the underlying tile.
 panel_screen::setTimeout(30);clickAt(8,110);step(31000);assert(!panel_screen::awake());clickAt(100,260);assert(panel_screen::awake()&&!overlay);clickAt(100,260);assert(overlay);dismiss();panel_screen::setTimeout(0);
 for(int p=0;p<4;p++){go(p);for(int i=0;i<4;i++){swipe();assert(page==(p+i+1)%4);}}
 for(int v:{Root,WifiView,BleView,DisplayView,WebView,ConnectionsView,HaView,ServiceView,StorageView,DiagnosticsView,AboutView,WifiMoreView}){go(PAGE_SETTINGS,v);if(v==DisplayView)save("display");if(v==BleView)save("bluetooth");check();}
 go(1,1);save("boiler");go(1,3);save("diesel");go(1,4);save("fuel-heater");
 go(0);
 auto contacts=[&](bool upperClosed,bool lowerClosed){
   const bool lowerReached=panel_config::reedLevelReached(panel_config::Type::ReedNC,!lowerClosed);
   const bool upperReached=panel_config::reedLevelReached(panel_config::Type::ReedNC,!upperClosed);
   for(unsigned i=4;i<6;i++){
     panel_sensors::Event event;event.revision=panel_config::fixture.revision;event.channel=uint8_t(i);
     event.atMs=millis();event.reedReached=i==4?lowerReached:upperReached;panel_sensors::queue.push_back(event);
   }
   measurements[3].put(34.8f,millis());step(1100);check();
 };
 contacts(false,false);assert(fuelLevel.status(millis())==FuelLevelStatus::UpperReached);
 assert(lv_obj_get_height(fuelCanister.fill)==CANISTER_FILL_HEIGHT&&!lv_obj_has_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN));
 assert(!strcmp(lv_label_get_text(fuelCanister.glyph),""));save("canister-full-fixture");
 contacts(true,false);assert(fuelLevel.status(millis())==FuelLevelStatus::Between);
 assert(lv_obj_get_height(fuelCanister.fill)==CANISTER_FILL_HEIGHT/2);save("canister-half-fixture");
 contacts(true,true);assert(fuelLevel.status(millis())==FuelLevelStatus::BelowLower);
 assert(lv_obj_has_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(fuelCanister.glyph),"!"));save("canister-empty-fixture");
 contacts(false,true);assert(fuelLevel.status(millis())==FuelLevelStatus::Inconsistent);
 assert(lv_obj_has_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(fuelCanister.glyph),"?"));save("canister-invalid-fixture");
 contacts(false,false);step(31000);updateLive();check();
 assert(lv_obj_has_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(fuelCanister.status),"Устарело"));save("canister-stale-fixture");
 fuelLevel.clear();updateLive();check();assert(!strcmp(lv_label_get_text(fuelCanister.status),"Нет уровня"));save("canister-unknown-fixture");
 fuelLevel.lower.put(true,millis());updateLive();assert(!strcmp(lv_label_get_text(fuelCanister.glyph),"?"));
 contacts(false,false);measurements[3].put(0,millis());updateLive();assert(!strcmp(lv_label_get_text(fuelCanister.temperature),"0.0 °C"));
 measurements[3].put(-55,millis());updateLive();check();assert(!strcmp(lv_label_get_text(fuelCanister.temperature),"-55.0 °C"));
 measurements[3].put(NAN,millis());updateLive();assert(!strcmp(lv_label_get_text(fuelCanister.temperature),"--.- °C"));
 // Icon children are passive: tapping its body still opens the fuel page.
 lv_area_t icon;lv_obj_get_coords(fuelCanister.body,&icon);clickAt((icon.x1+icon.x2)/2,(icon.y1+icon.y2)/2);
 assert(page==2&&!fuelCanister.root&&!overlay);touch("Домой");assert(fuelCanister.root);
 // Existing NO mode remains available for opposite-polarity float contacts.
 assert(panel_config::reedLevelReached(panel_config::Type::ReedNO,false));
 assert(!panel_config::reedLevelReached(panel_config::Type::ReedNO,true));
 for(int n=0;n<240;n++){go(n%4);}

 printf("RESULT: %u layout checks, clipped=%u; actual LVGL pointer navigation, back/home, fresh/absent/stale data, wake, AP selection race, pairing and physical SD gates; no hardware access\n",checks,clipped);
 return clipped?1:0;
#endif
}
