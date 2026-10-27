#include <assert.h>
#include <vector>
#include <string>
#include <fstream>
#include "Arduino.h"
#include "panel_radio.h"
#include "panel_screen.h"
uint32_t fakeMillis=1000;SerialStub Serial;ESPStub ESP;
namespace panel_radio {
panel::RadioState fixture;
panel::Command lastCommand;
bool begin(){return true;}
bool cancel(){return true;}
bool snapshot(panel::RadioState& out){out=fixture;return true;}
bool submit(const panel::Command& cmd){lastCommand=cmd;return true;}
const char* zoneName(uint8_t z){const char* zones[]={"UTC","Москва UTC+3","Берлин CET/CEST"};return zones[z<3?z:0];}
}
#include "../../src/main.cpp"
std::vector<lv_color_t> pixels(480*800);
void flush(lv_disp_drv_t* drv,const lv_area_t* a,lv_color_t* data){
 for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++)pixels[y*480+x]=*data++;
 lv_disp_flush_ready(drv);
}
int mx=0,my=0;bool down=false;
void pointer(lv_indev_drv_t*,lv_indev_data_t* d){d->point.x=mx;d->point.y=my;d->state=panel_screen_touch_ready() && panel_screen_filter_touch(down)?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
void step(int ms=200){for(int i=0;i<ms;i+=10){fakeMillis+=10;lv_tick_inc(10);lv_timer_handler();}}
void save(const char* name){
 lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);
 std::ofstream f(std::string("./")+name+".ppm",std::ios::binary);f<<"P6\n480 800\n255\n";
 for(auto p:pixels){const auto q=lv_color_to32(p);unsigned char rgb[3]={static_cast<unsigned char>((q>>16)&255),static_cast<unsigned char>((q>>8)&255),static_cast<unsigned char>(q&255)};f.write((char*)rgb,3);}
}
int clipped=0;
void bounds(lv_obj_t* obj,bool scroll=false){
 if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return;
 const bool inScroll=scroll || lv_obj_has_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
 if(lv_obj_check_type(obj,&lv_label_class) && !scroll){
  auto* parent=lv_obj_get_parent(obj);lv_area_t a,b;lv_obj_get_coords(obj,&a);lv_obj_get_coords(parent,&b);
  if(a.x1<b.x1 || a.x2>b.x2 || a.y1<b.y1 || a.y2>b.y2){printf("CLIP: %s | %d,%d,%d,%d parent %d,%d,%d,%d\n",lv_label_get_text(obj),a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);clipped++;}
 }
 for(unsigned i=0;i<lv_obj_get_child_cnt(obj);i++)bounds(lv_obj_get_child(obj,i),inScroll);
}
void clickAt(int x,int y){
 mx=x;my=y;down=true;step(60);down=false;
 // Physical release is sampled by the indev timer BEFORE the app's 100 ms
 // refresh consumes deferred navigation/dialog actions. A fixed 100 ms wait
 // races those independent timers; bound and assert completion instead.
 for(unsigned elapsed=10;elapsed<=250;elapsed+=10){
  step(10);auto* input=lv_indev_get_next(nullptr);
  if(input && input->proc.state==LV_INDEV_STATE_RELEASED && pendingPage<0 && !closeDialog && !rebuild){
   if(elapsed>100)printf("CLICK settled in %u ms after release\n",elapsed);
   return;
  }
 }
 fprintf(stderr,"CLICK timeout: pending=%d close=%d rebuild=%d\n",pendingPage,int(closeDialog),int(rebuild));
 assert(false && "UI did not settle within 250 ms after physical release");
}
void swipe(bool left){mx=left?400:80;my=650;down=true;step(70);for(int i=0;i<7;i++){mx+=(left?-40:40);step(35);}down=false;step(300);}
int main(){
 lv_init();static lv_color_t draw[480*40];static lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw,nullptr,480*40);
 static lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=480;dd.ver_res=800;dd.draw_buf=&buf;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 static lv_indev_drv_t id;lv_indev_drv_init(&id);id.type=LV_INDEV_TYPE_POINTER;id.read_cb=pointer;lv_indev_drv_register(&id);
 panel_radio::fixture.ready=true;panel_radio::fixture.wifiSupported=true;panel_radio::fixture.bleSupported=true;
 snprintf(panel_radio::fixture.wifiMessage,150,"Не подключено");snprintf(panel_radio::fixture.bleMessage,150,"Нажмите Поиск. Устройства не подключаются");
 auto* root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(root,0,0);
 lv_obj_add_event_cb(root,gesture,LV_EVENT_GESTURE,nullptr);panel_radio::snapshot(radio);
 panel_screen::begin(true);assert(panel_screen::touchReady());lv_timer_create(refresh,100,nullptr);page=0;render();step();save("home");bounds(root);
 // First physical gesture wakes only, even when over an actionable card.
 panel_radio::fixture.displayTimeoutSec=30;++panel_radio::fixture.revision;step(1100);step(31000);
 assert(!!panel_screen::awake() && hostBacklight); // No blank until a touch has actually worked this boot.
 clickAt(30,745);assert(panel_screen::awake());step(31000);
 assert(!panel_screen::awake() && !hostBacklight);const auto beforeClicks=clicks;
 mx=100;my=210;down=true;step(1500);
 assert(hostBacklight && !!panel_screen::awake() && !overlay && clicks==beforeClicks);
 mx=400;step(1500);down=false;step(200);assert(!overlay && page==0 && clicks==beforeClicks);
 clickAt(100,210);assert(overlay && dialogKind==3);swipe(true);assert(page==0);dismiss();
 panel_radio::fixture.displayTimeoutSec=0;++panel_radio::fixture.revision;step(1100);
 measurements[0].put(10,fakeMillis);measurements[0].put(12,fakeMillis+1);step(1100);
 assert(!strcmp(lv_label_get_text(tempValue[0]),"12.0"));assert(measurements[0].count==2);
 sensorDetails(0);auto* stableOverlay=overlay;step(31000);assert(overlay==stableOverlay);
 assert(strstr(lv_label_get_text(detailText),"устарели"));
 measurements[0].put(13,fakeMillis);step(1100);assert(overlay==stableOverlay);
 assert(strstr(lv_label_get_text(detailText),"свежие"));dismiss();
 auto* unchangedBody=body;step(31000);assert(body==unchangedBody);
 assert(!strcmp(lv_label_get_text(tempValue[0]),"--.-"));
 sensorDetails(0);step();save("history_test_fixture");bounds(lv_layer_top());dismiss();measurements[0]=Measurement{};
 // Exercise real LVGL pointer gestures and Home event callback.
 swipe(true);assert(page==1);swipe(true);assert(page==2);
 save("before_home_click");
 lv_point_t homePoint={398,45};auto* hit=lv_indev_search_obj(lv_scr_act(),&homePoint);
 printf("HOME BEFORE page=%d rendered=%d pending=%d rebuild=%d overlay=%p sleeping=%d consume=%d clicks=%lu hit=%p header=%p hit-button=%d\n",page,renderedPage,pendingPage,int(rebuild),(void*)overlay,int(!panel_screen::awake()),int(false),(unsigned long)clicks,(void*)hit,(void*)header,int(hit && lv_obj_check_type(hit,&lv_btn_class)));fflush(stdout);
 clickAt(398,45);
 if(page!=0){
   auto* in=lv_indev_get_next(nullptr);
   printf("HOME FAIL page=%d rendered=%d pending=%d rebuild=%d close=%d overlay=%p sleeping=%d consume=%d clicks=%lu input-state=%d wait-release=%d now=%lu\n",page,renderedPage,pendingPage,int(rebuild),int(closeDialog),(void*)overlay,int(!panel_screen::awake()),int(false),(unsigned long)clicks,in?int(in->proc.state):-1,in?int(in->proc.wait_until_release):-1,(unsigned long)fakeMillis);fflush(stdout);
   save("home_failure");
 }
 assert(page==0);
 for(int p=0;p<4;p++){page=p;view=0;render();step();std::string n="page"+std::to_string(p);save(n.c_str());bounds(root);}
 // Verify complete UTF-8 boot diagnostics, including the last field and worst-case widths.
 for(uint32_t value:{uint32_t(0),UINT32_MAX}){
  panel_screen::boot.setupAt=panel_screen::boot.displayMs=panel_screen::boot.lvglMs=panel_screen::boot.firstFrameAt=panel_screen::boot.touchMs=value;
  page=3;view=0;render();step();
  auto* timing=lv_obj_get_child(resultList,1);auto* timingText=lv_obj_get_child(timing,1);
  char expected[512];const auto v=static_cast<unsigned long>(value);
  const int bytes=snprintf(expected,sizeof(expected),"Начало программы: %lu мс\nДисплей: %lu мс   LVGL: %lu мс\nПервый кадр: %lu мс от старта\nСенсор после кадра: %lu мс",v,v,v,v,v);
  assert(bytes>0 && size_t(bytes)<sizeof(expected));
  assert(!strcmp(lv_label_get_text(timingText),expected));
  assert(!strchr(lv_label_get_text(timingText),'?'));
  bounds(timing); // Check this fixed card even though its ancestor is scrollable.
  lv_obj_scroll_to_y(resultList,350,LV_ANIM_OFF);step();
  save(value?"service_timing_max_test_fixture":"service_timing_zero");
 }
 panel_screen::boot={};
 page=2;for(int v=1;v<=4;v++){view=v;render();step();std::string n="settings"+std::to_string(v);save(n.c_str());bounds(root);}
 clickAt(100,310);assert(panel_radio::lastCommand.job==Job::SetDisplayTimeout);
 assert(panel_radio::lastCommand.displayTimeoutSec==30);
 credentials(nullptr);step();save("wifi_keyboard");bounds(lv_layer_top());
 lv_textarea_set_text(ssidField,"TestNetwork");lv_textarea_set_text(passwordField,"Password123");
 clickAt(300,410);assert(panel_radio::lastCommand.job==panel::Job::Connect);assert(!strcmp(panel_radio::lastCommand.ssid,"TestNetwork"));assert(!overlay);
 manualClock();step();save("clock_keyboard");bounds(lv_layer_top());dismiss();
 info("В котельной","Датчик не назначен.\n\nРеальных измерений и истории пока нет. Тип датчика и подключение зададим отдельно для этой котельной.");step();save("sensor_detail");bounds(lv_layer_top());dismiss();
 panel_radio::fixture.apCount=16;panel_radio::fixture.bleCount=16;
 for(int i=0;i<16;i++){
  auto& ap=panel_radio::fixture.aps[i];memset(ap.ssid,'W',32);ap.ssid[32]=0;ap.supported=true;ap.auth=3;ap.rssi=-45-i;
  auto& d=panel_radio::fixture.ble[i];memset(d.name,'W',63);d.name[63]=0;snprintf(d.address,sizeof(d.address),"AA:BB:CC:DD:EE:%02X",i);d.rssi=-55-i;
 }
 panel_radio::snapshot(radio);page=2;view=1;render();step();save("wifi_results_test_fixture");bounds(lv_scr_act());
 lv_obj_scroll_to_y(resultList,160,LV_ANIM_OFF);step();
 snprintf(panel_radio::fixture.aps[0].ssid,33,"ChangedSSID");++panel_radio::fixture.revision;step();
 assert(strstr(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(resultList,0),1)),"ChangedSSID"));
 assert(lv_obj_get_scroll_y(resultList)>0);
 lv_obj_scroll_to_y(resultList,0,LV_ANIM_OFF);step();mx=100;my=465;down=true;step(60);
 snprintf(panel_radio::fixture.aps[0].ssid,33,"ReplacementSSID");++panel_radio::fixture.revision;step(150);
 assert(!strcmp(displayedAps[0].ssid,"ChangedSSID"));
 down=false;step(150);assert(overlay && !strcmp(selectedSsid,"ChangedSSID"));dismiss();
 view=2;render();step();save("ble_results_test_fixture");bounds(lv_scr_act());
 panel_radio::fixture.busy=true;panel_radio::fixture.job=Job::BleScan;++panel_radio::fixture.revision;step();
 assert(activity && activityDot);save("busy_test_fixture");bounds(lv_scr_act());
 panel_radio::fixture.busy=false;++panel_radio::fixture.revision;step();
 for(int i=0;i<240;i++){page=i%4;view=page==2?(i/4)%4:0;render();step(10);}
 printf("RESULT: pointer swipes, Home, keyboard submit, wake consumption, freshness/history/modal/AP-race/scroll, 240 page rebuilds completed; clipped labels=%d\n",clipped);
 return clipped?1:0;
}
