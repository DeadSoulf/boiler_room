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
// Controlled epoch for repeatable previews and live rollover tests; no device clock.
time_t previewEpoch=0;
void setPreviewClock(int year,int month,int day,int hour,int minute,int second=0){
 tm t={};t.tm_year=year-1900;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;t.tm_sec=second;t.tm_isdst=-1;
 previewEpoch=mktime(&t);assert(previewEpoch!=time_t(-1));
}
time_t preview_time(time_t* out){if(out)*out=previewEpoch;return previewEpoch;}
#define time preview_time
#include UI_SOURCE
#undef time
std::vector<lv_color_t> pixels(480*800);
struct FrameStats {unsigned frames=0,fullFrames=0;uint64_t pixels=0;};
FrameStats frameStats;
void monitor(lv_disp_drv_t*,uint32_t,uint32_t px){++frameStats.frames;frameStats.pixels+=px;if(px==480U*800U)++frameStats.fullFrames;}
unsigned deletedScreenChildren=0;
void screenChildDeleted(lv_event_t* e){if(lv_event_get_target(e)==lv_scr_act())++deletedScreenChildren;}
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
void homeTemperatureChecks(){
 lv_obj_update_layout(lv_scr_act());
 auto* supply=lv_obj_get_parent(tempValue[2]);auto* backflow=lv_obj_get_parent(tempValue[4]);
 auto* coolant=lv_obj_get_parent(supply);
 assert(supply!=backflow&&coolant==lv_obj_get_parent(backflow)&&lv_obj_get_parent(coolant)==body);
 assert(lv_obj_get_child_cnt(body)==4U+(homeUpdateNotice?1U:0U));
 assert(lv_obj_get_child_cnt(coolant)==4&&lv_obj_get_width(coolant)==208&&lv_obj_get_height(coolant)==300);
 auto* heading=find(coolant,"Теплоноситель");assert(heading);
 auto* radiator=lv_obj_get_child(coolant,1);
 lv_area_t h,icon,first,second;
 lv_obj_get_coords(heading,&h);lv_obj_get_coords(radiator,&icon);
 lv_obj_get_coords(supply,&first);lv_obj_get_coords(backflow,&second);
 const auto* headingFont=lv_obj_get_style_text_font(heading,0);
 lv_point_t headingSize;lv_txt_get_size(&headingSize,lv_label_get_text(heading),headingFont,0,3,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 assert(headingSize.x<=lv_obj_get_width(heading)&&lv_obj_get_height(heading)==headingFont->line_height);
 assert(h.x2<icon.x1&&h.y2<first.y1&&icon.y2<first.y1&&first.y2<second.y1);
 auto* tank=lv_obj_get_parent(tempValue[3]);auto* outlet=lv_obj_get_parent(tempValue[5]);
 auto* fuel=lv_obj_get_parent(tank);auto* fuelHeading=lv_obj_get_parent(fuelCanister.root);
 assert(tank!=outlet&&fuel==lv_obj_get_parent(outlet)&&lv_obj_get_parent(fuel)==body&&fuel!=coolant);
 assert(lv_obj_get_child_cnt(fuel)==3&&lv_obj_get_width(fuel)==208&&lv_obj_get_height(fuel)==300);
 assert(lv_obj_get_parent(fuelHeading)==fuel&&find(fuelHeading,"Топливо")&&lv_obj_has_flag(fuelHeading,LV_OBJ_FLAG_CLICKABLE));
 lv_area_t fuelTitle,levelStatus;lv_obj_get_coords(find(fuelHeading,"Топливо"),&fuelTitle);
 lv_obj_get_coords(fuelCanister.status,&levelStatus);lv_obj_get_coords(fuelCanister.root,&icon);
 lv_obj_get_coords(tank,&first);lv_obj_get_coords(outlet,&second);
 assert(fuelTitle.y2<levelStatus.y1&&levelStatus.y2<first.y1&&levelStatus.x2<icon.x1&&icon.y2<first.y1&&first.y2<second.y1);
 for(unsigned i=0;i<CHANNELS;i++){
   auto* value=tempValue[i];assert(value&&lv_obj_get_style_text_font(value,0)==&boiler_font_55);
   auto* tile=lv_obj_get_parent(value);auto* unit=find(tile,"°C");assert(unit);
   lv_point_t textSize;lv_txt_get_size(&textSize,lv_label_get_text(value),&boiler_font_55,0,3,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
   assert(textSize.x<=lv_obj_get_width(value)&&lv_obj_get_height(value)==boiler_font_55.line_height);
   lv_area_t titleBox,valueBox,unitBox,statusBox,cardBox;
   lv_obj_get_coords(find(tile,SENSOR_NAMES[i]),&titleBox);lv_obj_get_coords(value,&valueBox);
   lv_obj_get_coords(unit,&unitBox);lv_obj_get_coords(tile,&cardBox);
   lv_obj_get_coords(tempState[i],&statusBox);
   assert(titleBox.y2<valueBox.y1&&valueBox.x1>=cardBox.x1&&valueBox.x2<=cardBox.x2&&statusBox.y2<cardBox.y2);
   if(i>=2){
     assert(lv_obj_get_style_border_width(tile,0)==0&&lv_obj_get_style_bg_opa(tile,0)==LV_OPA_TRANSP);
     assert(titleBox.x2<unitBox.x1&&unitBox.y2<valueBox.y1&&valueBox.y2<statusBox.y1);
   }else{
     lv_area_t iconBox;lv_obj_get_coords(tempIcon[i],&iconBox);
     assert(valueBox.y2<unitBox.y1&&unitBox.y2<iconBox.y1&&iconBox.y2<statusBox.y1);
     assert(iconBox.x1>=cardBox.x1&&iconBox.x2<=cardBox.x2);
   }
 }
}
void homeHeaderChecks(){
 assert(page==0&&headerTitle&&clockLabel&&dateLabel&&pairBadge&&pairDigits&&!find(header,"Домой"));
 lv_obj_update_layout(header);
 auto singleLine=[](lv_obj_t* object){
   const auto* font=lv_obj_get_style_text_font(object,0);lv_point_t textSize;
   lv_txt_get_size(&textSize,lv_label_get_text(object),font,lv_obj_get_style_text_letter_space(object,0),3,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
   assert(textSize.x<=lv_obj_get_width(object)&&lv_obj_get_height(object)==font->line_height);
 };
 singleLine(clockLabel);singleLine(dateLabel);singleLine(headerTitle);
 lv_area_t head,clock,date,pin,title,digits;
 lv_obj_get_coords(header,&head);lv_obj_get_coords(clockLabel,&clock);lv_obj_get_coords(dateLabel,&date);
 lv_obj_get_coords(pairBadge,&pin);lv_obj_get_coords(headerTitle,&title);lv_obj_get_coords(pairDigits,&digits);
 assert(clock.x1==date.x1&&clock.x2==date.x2&&clock.y2<date.y1);
 assert(clock.x1>pin.x2&&date.x1>pin.x2&&date.x2<head.x2&&clock.y1>head.y1&&date.y2<head.y2);
 if(!lv_obj_has_flag(pairBadge,LV_OBJ_FLAG_HIDDEN)){
   singleLine(pairDigits);assert(strlen(lv_label_get_text(pairDigits))==6&&lv_obj_get_style_text_font(pairDigits,0)==&boiler_font_25);
   assert(title.x2<pin.x1&&digits.x1>pin.x1&&digits.x2<pin.x2&&digits.y1>pin.y1&&digits.y2<pin.y2);
 }
 check();
}
void unchangedPixels(const std::vector<lv_color_t>& before,const std::vector<lv_area_t>& allowed={}){
 for(size_t i=0;i<pixels.size();++i)if(before[i].full!=pixels[i].full){
   const int x=int(i%480),y=int(i/480);bool expected=false;
   for(const auto& a:allowed)if(x>=a.x1&&x<=a.x2&&y>=a.y1&&y<=a.y2)expected=true;
   if(!expected){fprintf(stderr,"Unexpected pixel change at (%d,%d): %04x -> %04x\n",x,y,unsigned(before[i].full),unsigned(pixels[i].full));assert(false);}
 }
}
void homeClockChecks(bool fullRefresh){
 const time_t originalEpoch=previewEpoch;auto* originalHeader=header;auto* originalBody=body;
 auto* originalClock=clockLabel;auto* originalDate=dateLabel;auto* originalPin=pairDigits;
 auto expect=[&](const char* timeText,const char* dateText){
   step(1100);assert(header==originalHeader&&body==originalBody&&clockLabel==originalClock&&dateLabel==originalDate&&pairDigits==originalPin);
   assert(!strcmp(lv_label_get_text(clockLabel),timeText)&&!strcmp(lv_label_get_text(dateLabel),dateText));
   assert(!lv_obj_has_flag(pairBadge,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(pairDigits),"123456"));homeHeaderChecks();
 };
 setPreviewClock(2026,12,31,23,59,59);expect("23:59","31.12.2026");
 lv_coord_t longestClock=0;lv_point_t measured;
 for(int hour=0;hour<24;hour++)for(int minute=0;minute<60;minute++){
   char value[6];snprintf(value,sizeof(value),"%02d:%02d",hour,minute);
   lv_txt_get_size(&measured,value,&boiler_font_25,0,3,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
   if(measured.x>longestClock)longestClock=measured.x;
 }
 lv_txt_get_size(&measured,lv_label_get_text(clockLabel),&boiler_font_25,0,3,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 assert(measured.x==longestClock&&longestClock<=lv_obj_get_width(clockLabel));
 const int clockWidth=measured.x;
 lv_txt_get_size(&measured,lv_label_get_text(dateLabel),&boiler_font_19,0,3,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 const int dateWidth=measured.x;assert(lv_obj_get_style_text_font(dateLabel,0)==&boiler_font_19);
 lv_txt_get_size(&measured,lv_label_get_text(pairDigits),&boiler_font_25,0,3,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 lv_area_t clockBox,dateBox,pinBox;lv_obj_get_coords(clockLabel,&clockBox);lv_obj_get_coords(dateLabel,&dateBox);lv_obj_get_coords(pairBadge,&pinBox);
 printf("HOME_DATE geometry: time=(%d,%d,%d,%d) text=%d max1440=%d date=(%d,%d,%d,%d) text=%d PIN=(%d,%d,%d,%d) text=%d mode=%s\n",clockBox.x1,clockBox.y1,clockBox.x2,clockBox.y2,clockWidth,int(longestClock),dateBox.x1,dateBox.y1,dateBox.x2,dateBox.y2,dateWidth,pinBox.x1,pinBox.y1,pinBox.x2,pinBox.y2,int(measured.x),fullRefresh?"full-refresh":"partial");
 save("home-date-pin-fixture");
 ++previewEpoch;expect("00:00","01.01.2027");save("home-date-rollover-fixture");
 // Both strings follow the existing local timezone, including crossing midnight.
 setPreviewClock(2026,11,5,22,30);setenv("TZ","MSK-3",1);tzset();expect("01:30","06.11.2026");save("home-date-timezone-fixture");
 setenv("TZ","UTC0",1);tzset();expect("22:30","05.11.2026");
 for(time_t invalid:{time_t(-1),time_t(0),time_t(1704067199)}){previewEpoch=invalid;expect("--:--","--.--.----");}
 save("home-date-unknown-fixture");
 ++previewEpoch;expect("00:00","01.01.2024");
 previewEpoch=originalEpoch;expect("12:40","04.11.2026");
 // Isolate the header updater from other Home telemetry: unchanged strings need no redraw.
 lv_refr_now(nullptr);const auto unchanged=pixels;frameStats={};
 for(unsigned i=0;i<20;i++){updateClock();lv_refr_now(nullptr);}
 assert(!frameStats.frames);unchangedPixels(unchanged);
 printf("HOME_DATE live: invalid/recovery, year rollover, timezone, PIN retained, no rebuild; unchanged updater frames=%u mode=%s\n",frameStats.frames,fullRefresh?"full-refresh":"partial");
}
void updateRedrawChecks(bool fullRefresh){
 auto& update=panel_update::fixture;auto& network=panel_radio::fixture;
 const auto originalUpdate=update;const auto originalNetwork=network;
 step(500); // Let the real LVGL disabled-state transition finish once.
 auto* originalBody=body;auto* originalHeader=header;auto* originalFooter=footer;
 auto* originalProgress=updateProgress;auto* originalCancel=updateCancelButton;
 deletedScreenChildren=0;lv_obj_add_event_cb(lv_scr_act(),screenChildDeleted,LV_EVENT_CHILD_DELETED,nullptr);
 auto stable=[&](){assert(!deletedScreenChildren&&body==originalBody&&header==originalHeader&&footer==originalFooter&&updateProgress==originalProgress&&updateCancelButton==originalCancel);};
 const auto steadyPixels=pixels;frameStats={};step(1200);
 printf("OTA_REDRAW unchanged: frames=%u pixels=%llu\n",frameStats.frames,(unsigned long long)frameStats.pixels);
 assert(frameStats.frames==0);stable();unchangedPixels(steadyPixels);
 frameStats={};
 for(unsigned i=0;i<20;++i){
   network.rssi=-50-int(i%5);++network.revision;step(100);
   assert(radio.rssi==network.rssi&&radio.revision==network.revision);stable();unchangedPixels(steadyPixels);
 }
 // A connection change is still sampled, but this page has no radio-dependent content.
 network.connected=false;network.rssi=0;network.ip[0]=0;strcpy(network.wifiMessage,"Связь потеряна");++network.revision;step(120);
 assert(!radio.connected);stable();unchangedPixels(steadyPixels);
 network=originalNetwork;network.revision=radio.revision+1;step(120);assert(radio.connected);stable();unchangedPixels(steadyPixels);
 assert(frameStats.frames==0);
 printf("OTA_REDRAW radio: changes=22 frames=%u root_deletions=%u stable_pixels=yes\n",frameStats.frames,deletedScreenChildren);
 lv_area_t barBox,textBox;lv_obj_get_coords(lv_obj_get_parent(updateProgressFill),&barBox);lv_obj_get_coords(updateProgress,&textBox);
 const auto beforeProgress=pixels;frameStats={};
 for(unsigned percent=40;percent<=59;++percent){
   update.done=percent*100;update.percent=uint8_t(percent);step(120);stable();
   char expected[16];snprintf(expected,sizeof(expected),"%u%%",percent);assert(strstr(lv_label_get_text(updateProgress),expected));
   assert(lv_obj_get_width(updateProgressFill)==int(428U*percent/100U));
   assert(!lv_obj_has_state(updateCancelButton,LV_STATE_DISABLED));
   unchangedPixels(beforeProgress,{barBox,textBox});
 }
 assert(frameStats.frames>0);assert(!strcmp(lv_label_get_text(updateProgress),"59%   5900 Б / 10000 Б"));
 printf("OTA_REDRAW progress: updates=20 frames=%u full_frames=%u pixels=%llu mode=%s\n",frameStats.frames,frameStats.fullFrames,(unsigned long long)frameStats.pixels,fullRefresh?"full-refresh":"partial");
 // Bytes can change before the first percent; phase and controls must stay live too.
 update.total=100000;update.done=500;update.percent=0;step(120);assert(strstr(lv_label_get_text(updateProgress),"500 Б"));
 update.done=501;step(120);assert(strstr(lv_label_get_text(updateProgress),"501 Б")&&lv_obj_has_flag(updateProgressFill,LV_OBJ_FLAG_HIDDEN));
 update.phase=panel_update::Phase::Verifying;update.cancelAllowed=false;strcpy(update.message,"Проверяем файл обновления.");step(300);stable();
 assert(!strcmp(lv_label_get_text(updateStatus),"Проверка файла")&&!strcmp(lv_label_get_text(updateMessage),update.message));
 assert(lv_obj_has_state(updateCancelButton,LV_STATE_DISABLED)&&lv_obj_has_state(updateInstallButton,LV_STATE_DISABLED));
 // LONG_DOT stores an ellipsis in the label; it must not trigger repeat writes.
 std::string longMessage;while(longMessage.size()+strlen("Ш")<sizeof(update.message))longMessage+="Ш";
 strcpy(update.message,longMessage.c_str());step(300);assert(strcmp(lv_label_get_text(updateMessage),update.message));
 frameStats={};step(1000);assert(frameStats.frames==0);stable();
 update=originalUpdate;step(300);stable();assert(!strcmp(lv_label_get_text(updateStatus),"Загрузка")&&strstr(lv_label_get_text(updateProgress),"37%"));
 assert(!lv_obj_has_state(updateCancelButton,LV_STATE_DISABLED));
 lv_obj_remove_event_cb(lv_scr_act(),screenChildDeleted);
 // Leaving/re-entering still creates the right page; Wi-Fi must keep its live updates.
 network.connected=false;network.rssi=0;network.ip[0]=0;++network.revision;step(120);
 touch("<");touch("<");touch("Подключения");touch("Wi-Fi");assert(find(body,"Нет подключения"));
 const uint32_t nextRevision=network.revision+1;network=originalNetwork;network.revision=nextRevision;step(300);assert(find(body,"Подключён"));
 touch("<");touch("<");touch("Сервис");touch("Обновление");
 assert(strstr(lv_label_get_text(updateProgress),"37%")&&!strcmp(lv_label_get_text(updateMessage),originalUpdate.message));
 assert(!lv_obj_has_state(updateCancelButton,LV_STATE_DISABLED));
}
int main(int argc,char** argv){
 setvbuf(stdout,nullptr,_IOLBF,0);
 assert(argc==1||(argc==2&&!strcmp(argv[1],"--full-refresh")));const bool fullRefresh=argc==2;
 setenv("TZ","UTC0",1);tzset();setPreviewClock(2026,11,4,12,40);lv_init();
 static lv_color_t buf[480*800],second[480*800];static lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,buf,fullRefresh?second:nullptr,fullRefresh?480*800:480*40);
 lv_disp_drv_t display;lv_disp_drv_init(&display);display.hor_res=480;display.ver_res=800;display.flush_cb=flush;display.draw_buf=&draw;display.full_refresh=fullRefresh;display.monitor_cb=monitor;lv_disp_drv_register(&display);
 lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer;lv_indev_drv_register(&input);
 auto& radioFixture=panel_radio::fixture;radioFixture.ready=true;radioFixture.wifiSupported=true;radioFixture.bleSupported=true;radioFixture.connected=true;radioFixture.hasSaved=true;
#ifndef BEFORE_CAPTURE
 radioFixture.savedCount=2;strcpy(radioFixture.savedNetworks[0].ssid,"Example-WiFi");
 strcpy(radioFixture.savedNetworks[1].ssid,"Guest");radioFixture.savedNetworks[1].open=true;
#endif
 strcpy(radioFixture.ssid,"Example-WiFi");strcpy(radioFixture.ip,"192.0.2.10");radioFixture.rssi=-53;strcpy(radioFixture.hostVersion,"2.9.6");strcpy(radioFixture.c6Version,"2.9.6");
 auto& sd=panel_storage::fixture;sd.started=sd.mounted=sd.logging=true;sd.freeBytes=62602390528ULL;sd.usedBytes=6144;
 panel_ha::fixture.ready=panel_ha::fixture.supported=true;
 setup();step(400);check();save("home-no-data");
#ifdef BEFORE_CAPTURE
 for(int p=1;p<4;p++){go(p);save(p==1?"equipment-before":p==2?"settings-before":"service-before");}
 puts("PASS: baseline screenshots from actual old LVGL source, isolated fixtures");return 0;
#else
 assert(!strcmp(PAGES[2],"Топливо")&&!strcmp(PAGES[3],"Настройки"));
 homeHeaderChecks();assert(!strcmp(lv_label_get_text(clockLabel),"12:40")&&!strcmp(lv_label_get_text(dateLabel),"04.11.2026"));
 for(unsigned i=0;i<CHANNELS;i++){assert(tempValue[i]&&strstr(lv_label_get_text(tempValue[i]),"--"));assert(lv_obj_get_style_text_font(tempValue[i],0)==&boiler_font_55);assert(!freshBar[i]&&!tempTrend[i]);}
 homeTemperatureChecks();
 assert(!strcmp(SENSOR_NAMES[0],"В доме")&&find(body,"В доме")&&!find(body,"В котельной"));
 for(unsigned i=0;i<2;i++){
   assert(tempIcon[i]&&!lv_obj_has_flag(tempIcon[i],LV_OBJ_FLAG_CLICKABLE));
   auto* tile=lv_obj_get_parent(tempIcon[i]);lv_area_t pict,value,status,card;
   lv_obj_get_coords(tile,&card);lv_obj_get_coords(tempIcon[i],&pict);lv_obj_get_coords(tempValue[i],&value);lv_obj_get_coords(tempState[i],&status);
   lv_area_t unit;lv_obj_get_coords(find(tile,"°C"),&unit);
   assert(value.y2<unit.y1&&unit.y2<pict.y1&&pict.y2<status.y1&&pict.x1>=card.x1&&pict.x2<=card.x2);
   // All decorative descendants stay passive, including line widgets.
   std::vector<lv_obj_t*> nodes{tempIcon[i]};
   for(size_t n=0;n<nodes.size();n++){
     assert(!lv_obj_has_flag(nodes[n],LV_OBJ_FLAG_CLICKABLE));
     lv_area_t a;lv_obj_get_coords(nodes[n],&a);
     assert(a.x1>=pict.x1&&a.x2<=pict.x2&&a.y1>=pict.y1&&a.y2<=pict.y2);
     for(unsigned k=0;k<lv_obj_get_child_cnt(nodes[n]);k++)nodes.push_back(lv_obj_get_child(nodes[n],k));
   }
   clickAt((pict.x1+pict.x2)/2,(pict.y1+pict.y2)/2);assert(overlay&&targetEditor.channel==int(i));
   assert(find(overlay,SENSOR_NAMES[i])&&find(overlay,"Не задана")&&find(overlay,"Нет данных"));
   save(i==0?"room-no-data":"outside-no-data");touch("Назад");assert(!overlay&&page==0);
 }
 const char* emptyTemperatureScreens[]={"","","supply-no-data","tank-no-data","return-no-data","outlet-no-data"};
 for(unsigned i=2;i<CHANNELS;i++){
   auto* title=find(lv_obj_get_parent(tempValue[i]),SENSOR_NAMES[i]);
   lv_area_t bounds;lv_obj_get_coords(title,&bounds);
   clickAt((bounds.x1+bounds.x2)/2,(bounds.y1+bounds.y2)/2);
   assert(overlay&&targetEditor.channel==int(i));save(emptyTemperatureScreens[i]);touch("Назад");
 }
 // Exercise sensor events through the real non-contiguous GPIO to temperature mapping.
 auto sensorReading=[](uint8_t channel,float value){
   panel_sensors::Event event{};event.revision=panel_config::fixture.revision;
   event.channel=channel;event.temperature=value;event.atMs=millis();
   panel_sensors::queue.push_back(event);
 };
 sensorReading(2,60.5f);step(1100);
 assert(!strcmp(lv_label_get_text(tempValue[2]),"60.5")&&!strcmp(lv_label_get_text(tempValue[4]),"--.-"));
 sensorReading(6,40.0f);sensorReading(6,44.5f);step(1100);
 assert(measurements[2].count==1&&measurements[4].count==2&&!measurements[3].count);
 sensorReading(3,24.5f);step(1100);
 assert(!strcmp(lv_label_get_text(tempValue[3]),"24.5")&&!strcmp(lv_label_get_text(tempValue[5]),"--.-"));
 sensorReading(7,34.0f);sensorReading(7,39.5f);step(1100);
 assert(measurements[3].count==1&&measurements[5].count==2&&measurements[3].minimum==24.5f&&measurements[5].maximum==39.5f);
 const char* detailTemperatures[]={"","","60.5 °C","24.5 °C","44.5 °C","39.5 °C"};
 for(unsigned i=2;i<CHANNELS;i++){
   lv_area_t value;lv_obj_get_coords(tempValue[i],&value);
   clickAt((value.x1+value.x2)/2,(value.y1+value.y2)/2);
   assert(targetEditor.channel==int(i)&&find(overlay,SENSOR_NAMES[i]));
   assert(find(overlay,detailTemperatures[i])&&find(overlay,"Заданная температура"));
   assert(lv_obj_check_type(targetEditor.slider,&lv_slider_class)&&find(overlay,"Не задана"));
   assert(lv_obj_has_state(targetEditor.save,LV_STATE_DISABLED)&&!find(overlay,"Последние измерения"));
   touch("Назад");
 }
 // Drag the real LVGL slider. A release changes only the draft and Back discards it.
 touch("Подача");
 auto* slider=targetEditor.slider;
 assert(lv_slider_get_min_value(slider)==panel_targets::MIN_C&&lv_slider_get_max_value(slider)==panel_targets::MAX_C);
 assert(lv_slider_get_value(slider)==61);
 lv_area_t rail;lv_obj_get_coords(slider,&rail);
 mx=rail.x1+(61-panel_targets::MIN_C)*(rail.x2-rail.x1)/(panel_targets::MAX_C-panel_targets::MIN_C);
 my=(rail.y1+rail.y2)/2;down=true;step(80);
 for(int j=0;j<6;j++){mx+=10;step(50);}
 down=false;step(200);check();
 assert(overlay&&page==0&&lv_slider_get_value(slider)>61&&panel_targets::writes==0);
 touch("Назад");touch("Подача");
 assert(lv_slider_get_value(targetEditor.slider)==61&&find(overlay,"Не задана"));
 touch("+1 °C");assert(lv_slider_get_value(targetEditor.slider)==62&&find(overlay,"62 °C"));save("temperature-slider-fixture");
 sensorReading(2,62.5f);step(1100);
 assert(find(overlay,"62.5 °C")&&lv_slider_get_value(targetEditor.slider)==62);
 panel_update::fixture.busy=true;step(1100);
 assert(lv_obj_has_state(targetEditor.save,LV_STATE_DISABLED)&&panel_targets::writes==0);
 panel_update::fixture.busy=false;step(1100);touch("Сохранить");
 assert(!overlay&&page==0&&panel_targets::writes==1&&panel_targets::fixture[2].valueC==62);
 for(unsigned i=0;i<CHANNELS;i++)assert(panel_targets::fixture[i].configured==(i==2));
 touch("Подача");assert(lv_slider_get_value(targetEditor.slider)==62);touch("-1 °C");
 assert(panel_targets::save(2,1,57)==panel_targets::Result::Saved);step(1100);
 assert(lv_obj_has_state(targetEditor.save,LV_STATE_DISABLED)&&find(overlay,"61 °C"));
 touch("Назад");touch("Подача");assert(lv_slider_get_value(targetEditor.slider)==57);
 touch("Домой");assert(!overlay&&page==0&&panel_targets::writes==2);
 step(31000);sensorReading(2,61.5f);sensorReading(7,40.5f);step(1100);
 assert(!strcmp(lv_label_get_text(tempState[2]),"")&&!strcmp(lv_label_get_text(tempState[4]),"Устарело"));
 assert(!strcmp(lv_label_get_text(tempState[3]),"Устарело")&&!strcmp(lv_label_get_text(tempState[5]),""));
 touch("Обратка");
 assert(find(overlay,"Данные устарели")&&find(overlay,"Не задана")&&lv_slider_get_value(targetEditor.slider)==0);
 panel_targets::available=false;step(1100);assert(lv_obj_has_state(targetEditor.slider,LV_STATE_DISABLED));
 panel_targets::available=true;touch("Назад");
 for(float value:{-55.0f,0.0f,125.0f}){
   for(unsigned i=0;i<CHANNELS;i++){measurements[i].put(value,millis());}step(1100);check();homeTemperatureChecks();
   save(value<0?"home-font-negative-fixture":value>=100?"home-font-three-digit-fixture":"home-font-zero-fixture");
   touch("На улице");
   assert(lv_slider_get_value(targetEditor.slider)==int(value));
   assert(lv_obj_has_state(targetEditor.decrease,LV_STATE_DISABLED)==(value==panel_targets::MIN_C));
   assert(lv_obj_has_state(targetEditor.increase,LV_STATE_DISABLED)==(value==panel_targets::MAX_C));
   touch("Назад");
 }
 for(auto& reading:measurements){reading=Measurement{};}step(1100);check();homeTemperatureChecks();save("home-font-no-data-fixture");save("home-icons-no-data");
 assert(tempValue[3]&&tempValue[5]&&fuelCanister.root);
 assert(lv_obj_get_child_cnt(body)==4&&!find(body,"Уровень топлива"));
 assert(!strcmp(lv_label_get_text(tempValue[3]),"--.-"));
 assert(lv_obj_has_flag(fuelCanister.fill,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(fuelCanister.glyph),"?"));
 touch("Топливо");assert(page==2&&!overlay);touch("В баке");assert(overlay&&targetEditor.channel==3);save("temperature-no-data");touch("Назад");touch("Домой");assert(!overlay&&page==0);
 touch("В доме");touch("Пороги отопления");assert(overlay&&roomFields[0]);touch("Назад");
 swipe();assert(page==1&&view==0);save("equipment");
 touch("Автоматика");assert(page==1&&view==5);save("heating-automation");
 {
   using namespace panel_heating;
   using Channel=panel_config::Channel;
   auto& spec=panel_heating_settings::fixture;
   auto& sensor=panel_sensors::fixture;
   const auto oldSettings=spec;
   sensor.started=true;sensor.configRevision=panel_config::fixture.revision;
   spec.season=Season::Winter;++spec.revision;
   spec.config.roomForLoop[0]=1;spec.config.roomForLoop[8]=1;spec.config.roomForLoop[1]=2;
   spec.config.supplyTargetC=55;spec.config.returnTargetC=45;spec.config.returnRestartC=40;
   spec.config.tankOnC=40;spec.config.tankOffC=55;
   spec.config.coilStartDeltaC=8;spec.config.coilStopDeltaC=3;
   auto reading=[&](Channel id,float value,bool reached=false){
     auto& s=sensor.channels[unsigned(id)];
     s.valid=true;s.temperature=value;s.reached=reached;s.lastGoodMs=millis();
   };
   reading(Channel::Room,18);reading(Channel::Room2,24);
   reading(Channel::Coolant,52);reading(Channel::CoolantReturn,35);
   reading(Channel::TankWater,30);reading(Channel::CoilSupply,50);
   reading(Channel::Presence,0);reading(Channel::DieselFault,0);reading(Channel::ElectricFault,0);
   reading(Channel::PlantHeat,0,true);reading(Channel::Circulation,0,true);
   step(1100);
   panel_heating_runtime::Snapshot calc;
   assert(panel_heating_runtime::snapshot(calc)&&calc.settingsRevision==spec.revision);
   assert(calc.output.loop[0]==Flag::Yes&&calc.output.loop[8]==Flag::Yes&&calc.output.loop[1]==Flag::No);
   assert(calc.output.source==Source::Electric&&calc.output.stages==3&&calc.output.tankPower==Flag::No);
   assert(find(body,"9 кВт"));save("heating-automation-fixture");
   reading(Channel::Room,22);reading(Channel::Presence,0,true);reading(Channel::DieselFault,0,true);
   reading(Channel::Coolant,56);step(1100);
   assert(panel_heating_runtime::snapshot(calc)&&calc.output.source==Source::Electric&&calc.output.stages==2);
   assert(calc.output.loop[0]==Flag::Yes&&calc.output.loop[8]==Flag::No);
   assert(calc.output.tankPower==Flag::Yes&&calc.output.coilPump==Flag::Yes);
   assert(find(body,"6 кВт"));
   reading(Channel::CoolantReturn,45);step(1100);assert(panel_heating_runtime::snapshot(calc)&&calc.output.stages==0);
   sensor.channels[unsigned(Channel::Room)].lastGoodMs=millis()-panel::STALE_MS;
   step(1100);assert(panel_heating_runtime::snapshot(calc)&&calc.output.loop[0]==Flag::Unknown);
   assert(calc.output.loop[1]==Flag::No);
   ++sensor.configRevision;step(1100);
   assert(panel_heating_runtime::snapshot(calc)&&calc.input.presence==Presence::Unknown&&isnan(calc.input.tankC));
   touch("Пороги отопления");assert(roomFields[0]&&roomFields[2]);save("room-heating-thresholds");
   touch("Все +1 °C");assert(!strcmp(lv_textarea_get_text(roomFields[0]),"20.0")&&!strcmp(lv_textarea_get_text(roomFields[2]),"24.0"));
   touch("Все −1 °C");assert(!strcmp(lv_textarea_get_text(roomFields[1]),"22.0")&&spec.config.roomOnC==19);
   lv_textarea_set_text(roomFields[0],"23");touch("Сохранить");
   assert(overlay&&spec.config.roomOnC==19);
   lv_textarea_set_text(roomFields[0],"19,5");++spec.revision;touch("Сохранить");
   assert(overlay&&strstr(lv_label_get_text(dialogError),"изменились"));touch("Назад");
   touch("Пороги отопления");lv_textarea_set_text(roomFields[0],"19,5");touch("Сохранить");
   assert(!overlay&&spec.config.roomOnC==19.5f&&spec.config.roomForLoop[8]==1);
   sensor={};spec=oldSettings;step(1100);
 }
 touch("<");assert(page==1&&view==0);
 touch("Бойлер");assert(view==1&&find(body,"Температура воды в бойлере"));
 auto* blocked=lv_obj_get_parent(find(body,"Включить бойлер"));assert(lv_obj_has_state(blocked,LV_STATE_DISABLED));
 lv_area_t blockedAt;lv_obj_get_coords(blocked,&blockedAt);clickAt((blockedAt.x1+blockedAt.x2)/2,(blockedAt.y1+blockedAt.y2)/2);
 assert(!overlay&&!panel_boiler::attempts&&!strcmp(lv_label_get_text(boilerOnLabel),"Не задано"));
 touch("Изменить пороги");assert(overlay&&!*lv_textarea_get_text(boilerOnField)&&!*lv_textarea_get_text(boilerOffField));
 touch("Сохранить");assert(!panel_boiler::attempts&&!boilerConfirm);
 // Actual numeric keypad and field touches, including an invalid equal pair.
 clickAt(240,587);clickAt(240,587);clickAt(180,278);clickAt(240,587);clickAt(240,587);
 assert(!strcmp(lv_textarea_get_text(boilerOnField),"55")&&!strcmp(lv_textarea_get_text(boilerOffField),"55"));
 touch("Сохранить");assert(!panel_boiler::attempts&&!boilerConfirm);
 clickAt(350,747);clickAt(350,747);clickAt(385,587);clickAt(240,510);
 assert(!strcmp(lv_textarea_get_text(boilerOffField),"62"));
 panel_update::fixture.started=false;touch("Сохранить");assert(!boilerConfirm&&!panel_boiler::attempts);
 panel_update::fixture.started=true;
 touch("Сохранить");assert(boilerConfirm&&!panel_boiler::attempts);
 panel_update::fixture.busy=true;touch("Подтвердить");assert(!panel_boiler::attempts);
 panel_update::fixture.busy=false;panel_update::snapshotAvailable=false;touch("Подтвердить");assert(!panel_boiler::attempts);
 panel_update::snapshotAvailable=true;step(1200);assert(!strcmp(lv_textarea_get_text(boilerOffField),"62"));
 touch("Подтвердить");assert(!overlay&&panel_boiler::fixture.configured&&panel_boiler::attempts==1);
 assert(!strcmp(lv_label_get_text(boilerOnLabel),"55 °C и ниже")&&!strcmp(lv_label_get_text(boilerOffLabel),"62 °C и выше"));
 save("boiler-setpoints");
 touch("Изменить пороги");touch("Сохранить");
 ++panel_boiler::fixture.revision;panel_boiler::fixture.offC=64;step(1100);
 assert(!strcmp(lv_textarea_get_text(boilerOffField),"62"));touch("Подтвердить");
 assert(overlay&&strstr(lv_label_get_text(dialogError),"изменились")&&panel_boiler::fixture.offC==64);
 touch("Назад");assert(!overlay&&!strcmp(lv_label_get_text(boilerOffLabel),"64 °C и выше"));
 touch("<");assert(page==1&&view==0);
 touch("Электрокотёл");assert(page==1&&view==2);
 const char* heaterNames[]={"ТЭН-1","ТЭН-2","ТЭН-3"};
 lv_obj_t* heaters[3];
 const unsigned electricAttempts=panel_boiler::attempts;
 lv_area_t lastHeater{};
 for(unsigned i=0;i<3;i++){
   auto* caption=find(body,heaterNames[i]);assert(caption);
   heaters[i]=lv_obj_get_parent(caption);
   assert(lv_obj_check_type(heaters[i],&lv_btn_class)&&lv_obj_has_state(heaters[i],LV_STATE_DISABLED));
   auto* pin=find(heaters[i],"Без выхода");assert(pin);
   lv_area_t titleArea,pinArea;
   lv_obj_get_coords(caption,&titleArea);lv_obj_get_coords(pin,&pinArea);
   lv_obj_get_coords(heaters[i],&blockedAt);
   assert(titleArea.y2<pinArea.y1&&(!i||lastHeater.x2<blockedAt.x1));
   assert(blockedAt.x2-blockedAt.x1>=44&&blockedAt.y2-blockedAt.y1>=44);
   lastHeater=blockedAt;
   clickAt((blockedAt.x1+blockedAt.x2)/2,(blockedAt.y1+blockedAt.y2)/2);
   assert(!overlay&&panel_boiler::attempts==electricAttempts);
 }
 assert(find(body,"Подача")&&find(body,"Обратка")&&find(body,"Запрос"));
 assert(!strcmp(lv_label_get_text(boilerOnLabel),"Не задано")&&!strcmp(lv_label_get_text(boilerOffLabel),"Не задано"));
 save("electric");
 // The DHW pin is deliberately interleaved in the stored channel map.
 const auto priorPins=panel_config::fixture;
 auto bindRelay=[](panel_config::Channel channel,int gpio){
   auto& assignment=panel_config::fixture.item[unsigned(channel)];
   assignment.type=panel_config::Type::Relay;assignment.gpio=gpio;
 };
 bindRelay(panel_config::Channel::ElectricRelay,31);
 bindRelay(panel_config::Channel::DhwRelay,32);
 bindRelay(panel_config::Channel::ElectricRelay2,34);
 bindRelay(panel_config::Channel::ElectricRelay3,49);
 ++panel_config::fixture.revision;step(1100);
 assert(find(heaters[0],"GPIO 31")&&find(heaters[1],"GPIO 34")&&find(heaters[2],"GPIO 49"));
 assert(!find(body,"GPIO 32"));check();save("electric-three-heaters-fixture");
 panel_config::fixture.item[unsigned(panel_config::Channel::ElectricRelay2)]={};
 ++panel_config::fixture.revision;step(1100);
 assert(find(heaters[0],"GPIO 31")&&find(heaters[1],"Без выхода")&&find(heaters[2],"GPIO 49"));
 panel_config::available=false;step(1100);
 for(auto* heater:heaters)assert(find(heater,"Нет данных")&&lv_obj_has_state(heater,LV_STATE_DISABLED));
 panel_config::fixture=priorPins;panel_config::available=true;step(1100);
 for(auto* heater:heaters)assert(find(heater,"Без выхода"));
 touch("Изменить пороги");assert(find(overlay,"Пороги электрокотла"));
 assert(!*lv_textarea_get_text(boilerOnField)&&!*lv_textarea_get_text(boilerOffField));
 save("electric-editor");
 lv_textarea_set_text(boilerOnField,"42");lv_textarea_set_text(boilerOffField,"42");
 touch("Сохранить");assert(!boilerConfirm&&panel_boiler::attempts==electricAttempts);
 lv_textarea_set_text(boilerOffField,"58");touch("Сохранить");
 assert(boilerConfirm&&!panel_boiler::electric.configured);
 touch("Подтвердить");assert(!overlay&&panel_boiler::electric.revision==1);
 assert(!strcmp(lv_label_get_text(boilerOnLabel),"42 °C и ниже")&&!strcmp(lv_label_get_text(boilerOffLabel),"58 °C и выше"));
 assert(panel_boiler::fixture.onC==55&&panel_boiler::fixture.offC==64);
 for(auto name:heaterNames){
   auto* caption=find(body,name);assert(caption&&lv_obj_has_state(lv_obj_get_parent(caption),LV_STATE_DISABLED));
 }
 save("electric-setpoints-fixture");
 touch("Изменить пороги");assert(!strcmp(lv_textarea_get_text(boilerOffField),"58"));
 lv_textarea_set_text(boilerOnField,"40");touch("Сохранить");
 ++panel_boiler::electric.revision;panel_boiler::electric.onC=45;step(1100);
 touch("Подтвердить");assert(overlay&&strstr(lv_label_get_text(dialogError),"изменились"));
 touch("Назад");assert(!overlay&&!strcmp(lv_label_get_text(boilerOnLabel),"45 °C и ниже"));
 touch("<");assert(page==1&&view==0);
 touch("Дизельный отопитель");assert(view==3);touch("Домой");assert(page==0&&!overlay);
 swipe();swipe();assert(page==2);save("fuel-no-data");
 for(unsigned i:{3U,5U}){
   touch(SENSOR_NAMES[i]);assert(overlay&&targetEditor.channel==int(i));touch("Назад");assert(!overlay&&page==2);
 }
 swipe();assert(page==PAGE_SETTINGS);save("settings");touch("Подключения");assert(view==ConnectionsView);save("connections");
 touch("Wi-Fi");assert(view==WifiView);save("wifi");touch("Другие действия");assert(view==WifiMoreView);
 unsigned sent=panel_radio::submitted;touch("Забыть сеть");assert(overlay&&panel_radio::submitted==sent);save("forget-confirm");touch("Отмена");assert(!overlay&&view==WifiMoreView&&panel_radio::submitted==sent);
 touch("Отключить Wi-Fi");panel_radio::accept=false;touch("Отключить");assert(overlay&&dialogError&&strlen(lv_label_get_text(dialogError))>0);panel_radio::accept=true;touch("Отключить");assert(!overlay&&panel_radio::submitted==sent+1&&panel_radio::last.job==Job::Disconnect);
 // A changed password is a separate, explicit credentials flow.
 touch("Новый пароль");assert(overlay&&ssidLocked&&!openNetwork&&!strcmp(selectedSsid,"Example-WiFi"));
 assert(!*lv_textarea_get_text(passwordField));touch("Назад");
 // Forget must still name the exact profile confirmed, even after a snapshot reorder.
 touch("Забыть сеть");assert(find(overlay,"Example-WiFi"));sent=panel_radio::submitted;
 const auto firstProfile=radioFixture.savedNetworks[0];
 radioFixture.savedNetworks[0]=radioFixture.savedNetworks[1];radioFixture.savedNetworks[1]=firstProfile;
 ++radioFixture.revision;step(300);touch("Забыть сеть");
 assert(panel_radio::submitted==sent+1&&panel_radio::last.job==Job::Forget&&!panel_radio::last.open&&
        !strcmp(panel_radio::last.ssid,"Example-WiFi")&&!panel_radio::last.password[0]);
 // A reconnect pause is idle/offline, but the user must still be able to stop it.
 const auto connectedFixture=panel_radio::fixture;
 panel_radio::fixture.connected=false;panel_radio::fixture.busy=false;
 panel_radio::fixture.job=Job::None;panel_radio::fixture.autoConnect=true;
 ++panel_radio::fixture.revision;step(400);go(PAGE_SETTINGS,WifiMoreView);
 sent=panel_radio::submitted;touch("Отключить Wi-Fi");touch("Отмена");
 assert(!overlay&&panel_radio::submitted==sent);
 touch("Отключить Wi-Fi");touch("Отключить");
 assert(!overlay&&panel_radio::submitted==sent+1&&panel_radio::last.job==Job::Disconnect);
 panel_radio::fixture=connectedFixture;++panel_radio::fixture.revision;step(400);
 touch("<");assert(view==WifiView);touch("<");assert(view==ConnectionsView);
 touch("Home Assistant");assert(view==HaView);save("home-assistant");touch("<");assert(view==ConnectionsView);touch("<");assert(view==Root);
 // A browser request displays a code left of the clock, even from another page.
 const char* pairTicket="0123456789abcdef0123456789abcdef";
 assert(!find(lv_scr_act(),"Веб-доступ"));
 assert(panel_web::challenge.request(pairTicket,"123456",millis())==panel_pairing::Request::Started);
 step(300);assert(page==0&&pairBadge&&!lv_obj_has_flag(pairBadge,LV_OBJ_FLAG_HIDDEN));
 assert(!strcmp(lv_label_get_text(pairDigits),"123456"));
 lv_area_t pinBox,clockBox;lv_obj_get_coords(pairBadge,&pinBox);lv_obj_get_coords(clockLabel,&clockBox);assert(pinBox.x2<clockBox.x1);
 homeHeaderChecks();save("pairing-header-fixture");homeClockChecks(fullRefresh);
 assert(panel_web::challenge.verify(pairTicket,"000000",millis())==panel_pairing::Verify::Rejected);
 step(200);assert(!lv_obj_has_flag(pairBadge,LV_OBJ_FLAG_HIDDEN));
 assert(panel_web::challenge.verify(pairTicket,"123456",millis())==panel_pairing::Verify::Accepted);
 step(200);assert(lv_obj_has_flag(pairBadge,LV_OBJ_FLAG_HIDDEN)&&!strlen(lv_label_get_text(pairDigits)));
 homeHeaderChecks();save("home-date-no-pin-fixture");
 // A remote pairing request wakes the panel, but does not change the persisted timeout.
 step(31000);panel_screen::setTimeout(30);clickAt(8,110);step(31000);assert(!panel_screen::awake());
 assert(panel_web::challenge.request(pairTicket,"654321",millis())==panel_pairing::Request::Started);
 step(200);assert(panel_screen::awake()&&panel_screen::timeout()==30&&!lv_obj_has_flag(pairBadge,LV_OBJ_FLAG_HIDDEN));
 step(120100);assert(lv_obj_has_flag(pairBadge,LV_OBJ_FLAG_HIDDEN)&&!strlen(lv_label_get_text(pairDigits)));
 homeHeaderChecks();
 panel_screen::setTimeout(0);
 go(PAGE_SETTINGS);
 touch("Сервис");assert(view==ServiceView);save("service");touch("microSD");assert(view==StorageView);assert(lv_obj_has_flag(sdFormatAccept,LV_OBJ_FLAG_HIDDEN));save("storage");
 sd.formatPhase=panel_storage::FormatPhase::AwaitingPhysical;sd.formatRemainingSeconds=80;updateStorageLabel();check();save("storage-confirm-fixture");touch("Отмена");assert(panel_storage::cancelled==1&&panel_storage::accepted==0);
 sd.formatPhase=panel_storage::FormatPhase::AwaitingPhysical;updateStorageLabel();touch("Стереть microSD");assert(panel_storage::accepted==1);assert(lv_obj_has_flag(sdFormatAccept,LV_OBJ_FLAG_HIDDEN));
 touch("<");assert(view==ServiceView);touch("Диагностика");save("diagnostics");touch("<");touch("О панели");save("about");touch("Домой");assert(page==0);
 // Firmware update uses real pointer events, a pinned confirmation and live snapshots.
 assert(panel_update::heartbeats>0&&panel_update::healthy);
 startupServicesReady=false;step(120);assert(!panel_update::healthy);
 startupServicesReady=true;firstFrameShown=false;step(120);assert(!panel_update::healthy);
 firstFrameShown=true;panel_screen::begin(false);step(120);assert(!panel_update::healthy);
 panel_screen::begin(true);panel_screen::setTimeout(0);step(120);assert(panel_update::healthy);
 auto& update=panel_update::fixture;update.available=true;update.phase=panel_update::Phase::Available;
 strcpy(update.latestVersion,"v9.8.1");step(320);assert(homeUpdateNotice&&clockLabel&&!overlay);
 lv_area_t offer,lastCard;lv_obj_get_coords(homeUpdateNotice,&offer);lv_obj_get_coords(lv_obj_get_child(body,3),&lastCard);
 assert(offer.y1>lastCard.y2&&offer.y2<773);
 for(unsigned i=0;i<lv_obj_get_child_cnt(footer);i++)assert(!lv_obj_has_flag(lv_obj_get_child(footer,i),LV_OBJ_FLAG_CLICKABLE));
 save("update-offer-fixture");touch("Доступно обновление v9.8.1 >");assert(page==PAGE_SETTINGS&&view==UpdateView);
 assert(!strcmp(lv_label_get_text(updateLatest),"v9.8.1"));save("update-available-fixture");
 touch("Установить");assert(overlay&&dialogKind==5&&find(overlay,"v9.8.1")&&panel_update::installed==0);
 save("update-confirm-fixture");touch("Отмена");assert(!overlay&&panel_update::installed==0);
 touch("Установить");strcpy(update.latestVersion,"v9.8.2");step(120);touch("Установить");
 assert(overlay&&panel_update::installed==0&&strstr(lv_label_get_text(dialogError),"изменились"));touch("Отмена");
 touch("Установить");assert(find(overlay,"v9.8.2"));touch("Установить");
 assert(!overlay&&update.busy&&panel_update::installed==1&&!strcmp(panel_update::requestedVersion,"v9.8.2"));
 update.total=10000;update.done=3700;update.percent=37;strcpy(update.message,"Загружаем новую версию панели.");step(120);check();save("update-progress-fixture");
 assert(strstr(lv_label_get_text(updateProgress),"37%")&&lv_obj_has_state(updateInstallButton,LV_STATE_DISABLED));
 assert(!lv_obj_has_state(updateCancelButton,LV_STATE_DISABLED));updateRedrawChecks(fullRefresh);touch("Отменить обновление");
 assert(panel_update::cancelled==1&&!update.busy);assert(!strcmp(lv_label_get_text(updateStatus),"Обновление отменено"));
 update.busy=true;update.cancelAllowed=false;update.phase=panel_update::Phase::Installing;step(120);
 assert(lv_obj_has_state(updateCancelButton,LV_STATE_DISABLED));lv_area_t disabled;lv_obj_get_coords(updateCancelButton,&disabled);
 clickAt((disabled.x1+disabled.x2)/2,(disabled.y1+disabled.y2)/2);assert(panel_update::cancelled==1);
 touch("Домой");swipe();swipe();swipe();touch("Подключения");touch("Wi-Fi");
 const unsigned beforeBusy=panel_radio::submitted;touch("Поиск сетей");
 assert(overlay&&panel_radio::submitted==beforeBusy);touch("Закрыть");touch("<");touch("<");touch("Сервис");touch("Обновление");
 update.busy=false;update.phase=panel_update::Phase::Error;update.errorCode=19;
 strcpy(update.message,"Файл не прошёл проверку. Установленная версия сохранена. Проверьте соединение и повторите.");
 update.rollbackAvailable=true;strcpy(update.previousVersion,"v9.7.0");step(400);check();save("update-error-fixture");
 assert(strstr(lv_label_get_text(updateError),"19"));touch("Вернуть предыдущую версию");
 assert(overlay&&find(overlay,"v9.7.0")&&panel_update::rolledBack==0);strcpy(update.previousVersion,"v9.7.1");touch("Вернуть версию");
 assert(overlay&&panel_update::rolledBack==0);touch("Отмена");touch("Вернуть предыдущую версию");touch("Вернуть версию");
 assert(!overlay&&panel_update::rolledBack==1&&!strcmp(panel_update::requestedVersion,"v9.7.1"));
 update={};update.started=update.supported=true;step(120);touch("Проверить");
 assert(panel_update::checked==1&&update.phase==panel_update::Phase::Checking&&update.busy);
 update.busy=false;update.phase=panel_update::Phase::Current;step(120);assert(!strcmp(lv_label_get_text(updateStatus),"Версия актуальна"));
 touch("<");assert(view==ServiceView);touch("<");assert(view==Root);touch("Домой");assert(!homeUpdateNotice);
 go(PAGE_SETTINGS,WifiView);radioFixture.apCount=16;
 for(unsigned i=0;i<16;i++){auto& ap=radioFixture.aps[i];memset(ap.ssid,'W',32);ap.ssid[32]=0;ap.supported=true;ap.auth=3;ap.rssi=-50-int(i);}
 ++radioFixture.revision;step(300);check();save("wifi-results-fixture");
 lv_obj_scroll_to_y(resultList,160,LV_ANIM_OFF);step();assert(lv_obj_get_scroll_y(resultList)>0);
 ++radioFixture.revision;step();assert(lv_obj_get_scroll_y(resultList)>0);
 lv_obj_scroll_to_y(resultList,0,LV_ANIM_OFF);step();mx=150;my=375;down=true;step(80);
 strcpy(radioFixture.aps[0].ssid,"Replacement");++radioFixture.revision;step(140);assert(displayedAps[0].ssid[0]=='W');down=false;step(300);assert(overlay&&selectedSsid[0]=='W');check();save("credentials-fixture");dismiss();
 // An AP row is bound to the identity shown at press time. No credential editor
 // opens for a saved profile; a lookalike open AP never receives that credential.
 auto& known=radioFixture.aps[0];strcpy(known.ssid,"Example-WiFi");known.saved=true;
 ++radioFixture.revision;step(300);assert(find(resultList,"Сохранена / Защищённая сеть"));
 sent=panel_radio::submitted;panel_update::snapshotAvailable=false;clickAt(150,375);
 assert(overlay&&panel_radio::submitted==sent);panel_update::snapshotAvailable=true;dismiss();
 mx=150;my=375;down=true;step(80);
 known.auth=0;known.saved=false;++radioFixture.revision;step(140);down=false;step(300);
 assert(!overlay&&panel_radio::submitted==sent+1&&panel_radio::last.job==Job::Connect);
 assert(panel_radio::last.useSaved&&!panel_radio::last.open&&!strcmp(panel_radio::last.ssid,"Example-WiFi")&&!panel_radio::last.password[0]);
 clickAt(150,375);assert(overlay&&ssidLocked&&openNetwork&&panel_radio::submitted==sent+1);dismiss();
 go(0);const float values[CHANNELS]={22.4f,-8.2f,61.7f,34.8f,47.3f,41.5f};for(unsigned i=0;i<CHANNELS;i++)measurements[i].put(values[i],millis());fuelLevel.put(true,false,millis());step(1100);save("home-values-fixture");check();
 for(unsigned i=0;i<CHANNELS;i++)assert(!strcmp(lv_label_get_text(tempState[i]),""));
 assert(!strcmp(lv_label_get_text(tempValue[3]),"34.8")&&!strcmp(lv_label_get_text(tempValue[5]),"41.5"));homeTemperatureChecks();
 const char* sampleScreens[]={"room-values-fixture","outside-values-fixture","supply-values-fixture","tank-values-fixture","return-values-fixture","outlet-values-fixture"};
 for(unsigned i=0;i<CHANNELS;i++){
   touch(SENSOR_NAMES[i]);assert(overlay&&targetEditor.channel==int(i));
   save(sampleScreens[i]);touch("Назад");
 }
 go(2);assert(find(body,"34.8")&&find(body,"41.5"));save("fuel-values-fixture");go(0);step(31000);assert(!strcmp(lv_label_get_text(tempState[0]),"Устарело"));save("home-stale-fixture");
 // Screen wake consumes the first touch rather than activating the underlying tile.
 panel_screen::setTimeout(30);clickAt(8,110);step(31000);assert(!panel_screen::awake());clickAt(100,260);assert(panel_screen::awake()&&!overlay);clickAt(100,260);assert(overlay);dismiss();panel_screen::setTimeout(0);
 for(int p=0;p<4;p++){go(p);for(int i=0;i<4;i++){swipe();assert(page==(p+i+1)%4);if(page==0)homeHeaderChecks();else assert(!clockLabel&&!dateLabel&&find(header,"Домой"));}}
 for(int v:{Root,WifiView,BleView,DisplayView,ConnectionsView,HaView,ServiceView,StorageView,DiagnosticsView,AboutView,WifiMoreView,UpdateView}){go(PAGE_SETTINGS,v);if(v==DisplayView)save("display");if(v==BleView)save("bluetooth");check();}
 go(1,1);save("boiler");go(1,3);save("diesel");go(1,4);save("fuel-heater");
 for(unsigned i:{3U,5U}){touch(SENSOR_NAMES[i]);assert(overlay&&targetEditor.channel==int(i));touch("Назад");assert(!overlay&&page==1&&view==4);}
 touch("На выходе");touch("-1 °C");
 const int fuelSelection=lv_slider_get_value(targetEditor.slider);touch("Сохранить");
 assert(!overlay&&page==1&&view==4&&panel_targets::fixture[5].valueC==fuelSelection);
 touch("Домой");touch("Топливо");touch("На выходе");
 assert(targetEditor.channel==5&&lv_slider_get_value(targetEditor.slider)==fuelSelection);
 touch("Назад");assert(page==2&&!overlay);
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
 for(unsigned i=0;i<CHANNELS;i++)measurements[i].put(values[i],millis());
 contacts(false,false);save("home-icons-values-fixture");assert(fuelLevel.status(millis())==FuelLevelStatus::UpperReached);
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
 contacts(false,false);measurements[3].put(0,millis());updateLive();assert(!strcmp(lv_label_get_text(tempValue[3]),"0.0"));
 measurements[3].put(-55,millis());updateLive();check();assert(!strcmp(lv_label_get_text(tempValue[3]),"-55.0"));
 measurements[3].put(NAN,millis());updateLive();assert(!strcmp(lv_label_get_text(tempValue[3]),"--.-"));
 // Icon children are passive: tapping its body still opens the fuel page.
 lv_area_t icon;lv_obj_get_coords(fuelCanister.body,&icon);clickAt((icon.x1+icon.x2)/2,(icon.y1+icon.y2)/2);
 assert(page==2&&!fuelCanister.root&&!overlay);touch("Домой");assert(fuelCanister.root);
 // Existing NO mode remains available for opposite-polarity float contacts.
 assert(panel_config::reedLevelReached(panel_config::Type::ReedNO,false));
 assert(!panel_config::reedLevelReached(panel_config::Type::ReedNO,true));
 for(int n=0;n<240;n++){go(n%4);}

 printf("RESULT: %u layout checks, clipped=%u; Home date/clock/PIN fit, unknown time/recovery/rollover/timezone, all six Home temperatures font55 with no overlap (--.-, -55.0, 0.0, 125.0), temperature slider drag, 1-degree steps, save/cancel, independent channels and revision conflict, actual LVGL pointer navigation, update offer/confirmation/version race/progress/cancel/rollback/heartbeat, OTA redraw/pixel stability with radio changes, back/home, fresh/absent/stale data, wake, AP selection race, pairing and physical SD gates; no hardware access\n",checks,clipped);
 return clipped?1:0;
#endif
}
