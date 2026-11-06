// Read-only UI audit probe. The runner resolves host.cpp in --repo/tests/host.
#define main baseline_main
#include "host.cpp"
#undef main
#include <algorithm>
#include <cstring>

static int overlapCount=0, boundaryCount=0;
static void inspect(lv_obj_t* parent){
  if(lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN))return;
  std::vector<lv_obj_t*> labels;
  for(unsigned i=0;i<lv_obj_get_child_cnt(parent);++i){
    auto* ch=lv_obj_get_child(parent,i);
    if(lv_obj_check_type(ch,&lv_label_class) && lv_label_get_text(ch)[0]){
      labels.push_back(ch);
      if(!lv_obj_has_flag(parent,LV_OBJ_FLAG_SCROLLABLE)){
        lv_area_t a,b;lv_obj_get_coords(ch,&a);lv_obj_get_coords(parent,&b);
        if(a.x1<b.x1 || a.y1<b.y1 || a.x2>b.x2 || a.y2>b.y2){
          printf("BOUNDARY [%s] %d,%d,%d,%d parent %d,%d,%d,%d\n",lv_label_get_text(ch),a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);++boundaryCount;
        }
      }
    }
    inspect(ch);
  }
  for(size_t i=0;i<labels.size();++i)for(size_t j=i+1;j<labels.size();++j){
    lv_area_t a,b;lv_obj_get_coords(labels[i],&a);lv_obj_get_coords(labels[j],&b);
    const int w=std::min(a.x2,b.x2)-std::max(a.x1,b.x1)+1;
    const int h=std::min(a.y2,b.y2)-std::max(a.y1,b.y1)+1;
    if(w>0 && h>0){printf("OVERLAP %dx%d [%s] WITH [%s]\n",w,h,lv_label_get_text(labels[i]),lv_label_get_text(labels[j]));++overlapCount;}
  }
}
static void report(const char* scenario){
  step(500);lv_obj_update_layout(lv_scr_act());lv_obj_update_layout(lv_layer_top());
  overlapCount=boundaryCount=0;printf("\nSCENARIO %s\n",scenario);
  inspect(overlay?lv_layer_top():lv_scr_act());
  printf("RESULT overlaps=%d boundaries=%d\n",overlapCount,boundaryCount);save(scenario);
}
static void initialize(){
  lv_init();static lv_color_t draw[480*40];static lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw,nullptr,480*40);
  static lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=480;dd.ver_res=800;dd.draw_buf=&buf;dd.flush_cb=flush;lv_disp_drv_register(&dd);
  static lv_indev_drv_t id;lv_indev_drv_init(&id);id.type=LV_INDEV_TYPE_POINTER;id.read_cb=pointer;lv_indev_drv_register(&id);
  auto* root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(root,0,0);lv_obj_add_event_cb(root,gesture,LV_EVENT_GESTURE,nullptr);
  panel_screen::begin(true);lv_timer_create(refresh,100,nullptr);
  panel_radio::fixture.ready=true;panel_radio::fixture.wifiSupported=true;panel_radio::fixture.bleSupported=true;
  panel_radio::snapshot(radio);page=0;render();
}
static void setRadio(int newView){
  ++panel_radio::fixture.revision;panel_radio::snapshot(radio);page=2;view=newView;render();
}
int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  int behaviorFailures=0;
  initialize();report("default_home");
  panel_radio::fixture.connected=true;memset(panel_radio::fixture.ssid,'W',32);panel_radio::fixture.ssid[32]=0;
  snprintf(panel_radio::fixture.ip,sizeof(panel_radio::fixture.ip),"192.168.100.100");panel_radio::fixture.rssi=-80;
  snprintf(panel_radio::fixture.wifiMessage,sizeof(panel_radio::fixture.wifiMessage),"Подключено. Время синхронизируется");
  setRadio(1);report("connected_32_W_ssid");
  AccessPoint ap;memset(ap.ssid,'W',32);ap.ssid[32]=0;ap.supported=true;ap.auth=3;
  credentials(&ap);report("selected_32_W_ssid");dismiss();
  snprintf(ap.ssid,sizeof(ap.ssid),"Line1\nLine2\nLine3\nLine4\nLine5");
  credentials(&ap);report("selected_multiline_ssid");dismiss();
  panel_radio::fixture.connected=false;panel_radio::fixture.ssid[0]=0;
  snprintf(panel_radio::fixture.wifiMessage,sizeof(panel_radio::fixture.wifiMessage),"Ошибка поиска: код -2, 15000 мс. Нужен журнал C6");
  setRadio(1);report("wifi_scan_error");
  snprintf(panel_radio::fixture.bleMessage,sizeof(panel_radio::fixture.bleMessage),"Поиск завершён. Только BLE, без подключения");
  setRadio(2);report("ble_complete");
  panel_radio::fixture.bleSupported=false;
  snprintf(panel_radio::fixture.bleMessage,sizeof(panel_radio::fixture.bleMessage),"BLE не включён в установленной сборке Arduino");
  setRadio(2);report("ble_unavailable");
  credentials(nullptr);step(200);
  clickAt(38,500);assert(!strcmp(lv_textarea_get_text(ssidField),"q"));
  clickAt(38,742);clickAt(38,500);assert(!strcmp(lv_textarea_get_text(ssidField),"qQ"));
  clickAt(183,742);printf("KEYBOARD map symbols=%d selected=%u text_before_digit=[%s]\n",int(symbols),lv_btnmatrix_get_selected_btn(keyboard),lv_textarea_get_text(ssidField));clickAt(38,490);printf("KEYBOARD text_after_digit=[%s]\n",lv_textarea_get_text(ssidField));save("keyboard_digit_probe");
  if(strcmp(lv_textarea_get_text(ssidField),"qQ1")){++behaviorFailures;puts("FAIL switching to symbols must not type a character");}
  const std::string typed=lv_textarea_get_text(ssidField);
  auto* heldKeyboard=keyboard;auto* heldOverlay=overlay;
  for(int i=0;i<20;i++){++panel_radio::fixture.revision;step(110);}
  assert(heldKeyboard==keyboard && heldOverlay==overlay && lv_textarea_get_text(ssidField)==typed);
  printf("PASS keyboard input survives 20 radio revisions\n");
  clickAt(60,750);printf("KEYBOARD return_to_abc upper=%d symbols=%d text=[%s]\n",int(upper),int(symbols),lv_textarea_get_text(ssidField));
  if(upper || symbols || lv_textarea_get_text(ssidField)!=typed){++behaviorFailures;puts("FAIL abc must return to lowercase without entering characters");}
  dismiss();
  credentials(nullptr);step(200);clickAt(183,742);printf("KEYBOARD lowercase_to_symbols text=[%s]\n",lv_textarea_get_text(ssidField));
  if(lv_textarea_get_text(ssidField)[0]){++behaviorFailures;puts("FAIL direct lowercase-to-symbol switch inserted text");}dismiss();
  page=0;view=0;render();measurements[0].put(21.5f,fakeMillis);step(1100);assert(!strcmp(lv_label_get_text(tempValue[0]),"21.5"));
  step(30000);printf("STALE count=%u state=[%s] summary=[%s] hint=[%s]\n",measurements[0].count,lv_label_get_text(tempState[0]),lv_label_get_text(sensorSummary),lv_label_get_text(sensorHint));
  report("stale_sensor_summary");
  panel_radio::fixture.ready=false;panel_radio::fixture.wifiSupported=false;panel_radio::fixture.bleSupported=false;panel_radio::fixture.hostVersion[0]=0;panel_radio::fixture.c6Version[0]=0;
  panel_radio::snapshot(radio);page=3;view=0;render();report("radio_not_ready_service");
  panel_radio::fixture.ready=true;panel_radio::fixture.wifiSupported=true;panel_radio::fixture.bleSupported=true;
  snprintf(panel_radio::fixture.hostVersion,sizeof(panel_radio::fixture.hostVersion),"2.9.6");snprintf(panel_radio::fixture.c6Version,sizeof(panel_radio::fixture.c6Version),"2.9.6");++panel_radio::fixture.revision;
  step(2000);report("radio_now_ready_service");
  setenv("TZ","CET-1CEST,M3.5.0/2,M10.5.0/3",1);tzset();time_t parsed;
  assert(!parseLocalTime("2026-03-29 02:30",parsed));assert(parseLocalTime("2026-03-29 03:30",parsed));assert(parseLocalTime("2026-10-25 02:30",parsed));
  setenv("TZ","MSK-3",1);tzset();assert(parseLocalTime("2026-03-29 02:30",parsed));
  puts("PASS manual-date DST gap rejection and valid Berlin/Moscow dates on native libc");
  printf("COMPLETE targeted UI probes; behavior failures=%d\n",behaviorFailures);
  return behaviorFailures?1:0;
}
