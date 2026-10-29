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
void clickAt(int x,int y,int heldMs=60){
 mx=x;my=y;down=true;step(heldMs);down=false;
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
void clickKey(const char* text,int heldMs=60){
 // Find the visible key, then exercise the real LVGL pointer path.
 lv_obj_update_layout(keyboard);lv_area_t origin;lv_obj_get_coords(keyboard,&origin);
 const auto* matrix=reinterpret_cast<const lv_btnmatrix_t*>(keyboard);
 for(uint16_t i=0;const char* key=lv_btnmatrix_get_btn_text(keyboard,i);++i){
  if(strcmp(key,text))continue;
  const auto& area=matrix->button_areas[i];
  clickAt(origin.x1+(area.x1+area.x2)/2,origin.y1+(area.y1+area.y2)/2,heldMs);return;
 }
 assert(false && "Keyboard key not found");
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
 // Stale measurements still have history; the summary must not claim none ever arrived.
 assert(measurements[0].count==3);
 assert(!strcmp(lv_label_get_text(tempState[0]),"Нет свежих данных"));
 assert(!strcmp(lv_label_get_text(tempTrend[0]),"История: нажмите"));
 assert(!strcmp(lv_label_get_text(sensorSummary),"Свежие температуры: 0 / 4"));
 assert(!strcmp(lv_label_get_text(sensorHint),"Свежих температурных данных пока нет"));
 sensorDetails(0);step();save("history_test_fixture");bounds(lv_layer_top());dismiss();measurements[0]=Measurement{};step(1100);
 assert(!strcmp(lv_label_get_text(tempState[0]),"Нет данных"));
 assert(!strcmp(lv_label_get_text(tempTrend[0]),"Истории пока нет"));
 // Fuel level uses independent normalized reed thresholds; no hardware data is invented.
 assert(fuelLevel.status(fakeMillis)==FuelLevelStatus::NoData);
 assert(!strcmp(lv_label_get_text(fuelCardLabels.summary),"Нет данных"));
 assert(!strcmp(lv_label_get_text(fuelCardLabels.upper),"Нет данных"));
 assert(!strcmp(lv_label_get_text(fuelCardLabels.lower),"Нет данных"));
 save("fuel_home_no_data");
 clickAt(360,440);assert(overlay && dialogKind==4);step();save("fuel_detail_no_data");bounds(lv_layer_top());
 const auto stableFuelOverlay=overlay;
 struct FuelCase {bool lower,upper;FuelLevelStatus status;const char* text;const char* screenshot;};
 const FuelCase fuelCases[]={
  {false,false,FuelLevelStatus::BelowLower,"Ниже нижнего","fuel_below_test_fixture"},
  {true,false,FuelLevelStatus::Between,"Между датчиками","fuel_between_test_fixture"},
  {true,true,FuelLevelStatus::UpperReached,"Верхний достигнут","fuel_upper_test_fixture"},
  {false,true,FuelLevelStatus::Inconsistent,"Проверьте датчики","fuel_inconsistent_test_fixture"}
 };
 for(const auto& sample:fuelCases){
  fuelLevel.put(sample.lower,sample.upper,fakeMillis);step(1100);
  assert(overlay==stableFuelOverlay && measurements[3].count==0);
  assert(fuelLevel.status(fakeMillis)==sample.status);
  assert(!strcmp(lv_label_get_text(fuelDetailLabels.summary),sample.text));
  assert(!strcmp(lv_label_get_text(fuelCardLabels.summary),sample.text));
  assert(!strcmp(lv_label_get_text(fuelDetailLabels.upper),sample.upper?"Достигнут":"Не достигнут"));
  assert(!strcmp(lv_label_get_text(fuelDetailLabels.lower),sample.lower?"Достигнут":"Не достигнут"));
  save(sample.screenshot);bounds(lv_layer_top());
 }
 fuelLevel.put(true,false,fakeMillis);step(31000);
 assert(overlay==stableFuelOverlay);
 assert(!strcmp(lv_label_get_text(fuelDetailLabels.summary),"Данные устарели"));
 assert(!strcmp(lv_label_get_text(fuelDetailLabels.upper),"Устарело"));
 save("fuel_stale_test_fixture");bounds(lv_layer_top());
 fuelLevel.upper.put(true,fakeMillis);step(1100);
 assert(!strcmp(lv_label_get_text(fuelDetailLabels.summary),"Данные устарели"));
 assert(!strcmp(lv_label_get_text(fuelDetailLabels.upper),"Достигнут"));
 fuelLevel.lower.clear();step(1100);
 assert(!strcmp(lv_label_get_text(fuelDetailLabels.summary),"Нет данных"));
 assert(!strcmp(lv_label_get_text(fuelDetailLabels.lower),"Нет данных"));
 fuelLevel.put(true,false,fakeMillis);measurements[3].put(18.4f,fakeMillis);step(1100);
 assert(!strcmp(lv_label_get_text(fuelTemperatureValue),"18.4 °C"));
 clickAt(220,600);assert(dialogKind==3 && detailChannel==3);
 clickAt(200,680);assert(dialogKind==4 && fuelDetailLabels.summary);
 assert(!strcmp(lv_label_get_text(fuelDetailLabels.summary),"Между датчиками"));
 clickAt(400,40);assert(!overlay && page==0 && !fuelDetailLabels.summary && !fuelTemperatureValue);
 assert(!strcmp(lv_label_get_text(tempValue[3]),"18.4"));
 assert(lv_obj_get_style_text_font(tempValue[3],LV_PART_MAIN)==&boiler_font_25);
 step();save("fuel_home_between_test_fixture");bounds(root);
 // Inspect each compact card state after a normal live update, including long status text.
 for(const auto& sample:fuelCases){fuelLevel.put(sample.lower,sample.upper,fakeMillis);step(1100);bounds(lv_obj_get_parent(fuelCardLabels.summary));}
 fuelLevel.clear();measurements[3]=Measurement{};step(1100);
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
 clickKey("q");clickKey("ABC");clickKey("Q");clickKey("123");
 assert(!strcmp(lv_textarea_get_text(ssidField),"qQ"));
 clickKey("1");assert(!strcmp(lv_textarea_get_text(ssidField),"qQ1"));
 clickKey("abc");assert(!upper && !symbols);
 assert(!strcmp(lv_textarea_get_text(ssidField),"qQ1"));
 clickKey("q");assert(!strcmp(lv_textarea_get_text(ssidField),"qQ1q"));
 // A held layout key must neither repeat a layout switch nor type from its new map.
 clickKey("123",900);assert(symbols && !strcmp(lv_textarea_get_text(ssidField),"qQ1q"));
 clickKey("abc",900);assert(!upper && !symbols && !strcmp(lv_textarea_get_text(ssidField),"qQ1q"));
 clickKey("ABC",900);assert(upper && !symbols && !strcmp(lv_textarea_get_text(ssidField),"qQ1q"));
 clickKey("abc");
 lv_textarea_set_text(ssidField,"");clickKey("123");assert(!lv_textarea_get_text(ssidField)[0]);
 clickKey("abc");assert(!upper && !symbols && !lv_textarea_get_text(ssidField)[0]);
 clickAt(80,232);assert(activeField==passwordField && lv_textarea_get_password_mode(passwordField));
 clickKey("a");clickKey("ABC");clickKey("A");clickKey("123");clickKey("1");clickKey("abc");clickKey("b");
 assert(!strcmp(lv_textarea_get_text(passwordField),"aA1b"));
 assert(!upper && !symbols && !lv_textarea_get_text(ssidField)[0]);
 save("keyboard_layout_regression");
 lv_textarea_set_text(ssidField,"TestNetwork");lv_textarea_set_text(passwordField,"Password123");
 clickAt(300,410);assert(panel_radio::lastCommand.job==panel::Job::Connect);assert(!strcmp(panel_radio::lastCommand.ssid,"TestNetwork"));assert(!overlay);
 const char* selectedNames[]={"Line1\nLine2\nLine3\nLine4\nLine5","Room\r\n\tBoiler","WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"};
 for(const char* name:selectedNames){
  AccessPoint ap;snprintf(ap.ssid,sizeof(ap.ssid),"%s",name);ap.auth=3;ap.supported=true;
  credentials(&ap);step();
  auto* shownName=lv_obj_get_child(overlay,3);auto* passwordLabel=lv_obj_get_child(overlay,4);
  assert(lv_obj_check_type(shownName,&lv_label_class) && lv_obj_check_type(passwordLabel,&lv_label_class));
  assert(!strcmp(lv_label_get_text(passwordLabel),"Пароль"));
  lv_area_t nameBox,passwordBox;lv_obj_get_coords(shownName,&nameBox);lv_obj_get_coords(passwordLabel,&passwordBox);
  assert(nameBox.y2<passwordBox.y1);
  assert(!strchr(lv_label_get_text(shownName),'\n') && !strchr(lv_label_get_text(shownName),'\r') && !strchr(lv_label_get_text(shownName),'\t'));
  assert(!strcmp(selectedSsid,name));bounds(lv_layer_top());
  if(strchr(name,'\n'))save("selected_multiline_ssid_regression");
  lv_textarea_set_text(passwordField,"Password123");clickAt(300,410);
  assert(!overlay && panel_radio::lastCommand.job==Job::Connect && !strcmp(panel_radio::lastCommand.ssid,name));
 }
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
 printf("RESULT: pointer swipes, Home, keyboard layouts/held keys/password submit, bounded SSID/raw connection, wake consumption, freshness/history/modal/AP-race/scroll, fuel thresholds/unknown/stale/history, 240 page rebuilds completed; clipped labels=%d\n",clipped);
 return clipped?1:0;
}
