// Lifecycle test for the patched, pinned OneWire32 implementation. Each ESP-IDF
// allocation and enable point can fail. A successful TX stays pending until disabled.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#define IRAM_ATTR
constexpr int ESP_OK=0, RMT_CLK_SRC_DEFAULT=0, pdFALSE=0, pdTRUE=1;
constexpr int pdMS_TO_TICKS(int ms){return ms;}
using BaseType_t=int;
using gpio_num_t=int;
struct Handle {bool allocated=false,enabled=false;};
using rmt_channel_handle_t=Handle*;
using rmt_encoder_handle_t=Handle*;
using QueueHandle_t=Handle*;
struct rmt_symbol_word_t {int duration0,level0,duration1,level1;};
struct rmt_tx_flags {int eot_level;};
struct rmt_transmit_config_t {int loop_count;rmt_tx_flags flags;};
struct rmt_receive_config_t {int signal_range_min_ns,signal_range_max_ns;};
struct rmt_bytes_encoder_config_t {rmt_symbol_word_t bit0,bit1;struct {int msb_first;} flags;};
struct rmt_copy_encoder_config_t {};
struct rmt_rx_channel_config_t {int gpio_num,clk_src,resolution_hz,mem_block_symbols;};
struct rmt_tx_channel_config_t {int gpio_num,clk_src,resolution_hz,mem_block_symbols,trans_queue_depth;struct {int io_loop_back,io_od_mode;} flags;};
struct rmt_rx_done_event_data_t {size_t num_symbols;rmt_symbol_word_t* received_symbols;};
struct rmt_rx_event_callbacks_t {bool (*on_recv_done)(Handle*,const rmt_rx_done_event_data_t*,void*);};
Handle bytes,copy,rx,tx,queue;
unsigned failAt=0,step=0;
bool txPending=false;
bool succeeds(){return ++step!=failAt;}
int allocate(Handle& resource,Handle** out){
    assert(!resource.allocated);
    if(!succeeds())return -1;
    resource.allocated=true;*out=&resource;return ESP_OK;
}
int rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t*,Handle** out){return allocate(bytes,out);}
int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*,Handle** out){return allocate(copy,out);}
int rmt_new_rx_channel(const rmt_rx_channel_config_t*,Handle** out){return allocate(rx,out);}
int rmt_new_tx_channel(const rmt_tx_channel_config_t*,Handle** out){return allocate(tx,out);}
QueueHandle_t xQueueCreate(unsigned,size_t){
    Handle* result=nullptr;allocate(queue,&result);return result;
}
int rmt_rx_register_event_callbacks(Handle* ch,const rmt_rx_event_callbacks_t*,void* q){
    assert(ch==&rx&&q==&queue);return succeeds()?ESP_OK:-1;
}
int rmt_enable(Handle* h){
    assert(h->allocated&&!h->enabled);
    if(!succeeds())return -1;
    h->enabled=true;return ESP_OK;
}
int rmt_transmit(Handle* h,Handle* enc,const void*,size_t,const rmt_transmit_config_t*){
    assert(h==&tx&&tx.enabled&&enc->allocated);txPending=true;return ESP_OK;
}
int rmt_disable(Handle* h){
    assert((h==&tx||h==&rx)&&h->allocated&&h->enabled);
    h->enabled=false;if(h==&tx)txPending=false;return ESP_OK;
}
int rmt_del_channel(Handle* h){
    assert((h==&tx||h==&rx)&&h->allocated&&!h->enabled);h->allocated=false;return ESP_OK;
}
int rmt_del_encoder(Handle* h){
    assert((h==&bytes||h==&copy)&&h->allocated&&!txPending);h->allocated=false;return ESP_OK;
}
void vQueueDelete(Handle* h){
    assert(h==&queue&&h->allocated&&!rx.allocated);h->allocated=false;
}
int xQueueSendFromISR(QueueHandle_t,const void*,BaseType_t*){return 0;}
int rmt_receive(Handle*,void*,size_t,const rmt_receive_config_t*){return -1;}
int xQueueReceive(QueueHandle_t,void*,int){return pdFALSE;}
int rmt_tx_wait_all_done(Handle*,int){return -1;}

#include "OneWireESP32.cpp"

int main(){
    alignas(OneWire32) unsigned char storage[sizeof(OneWire32)];
    for(failAt=1;failAt<=9;failAt++){
        step=0;std::memset(storage,0xa5,sizeof(storage));
        auto* bus=new(storage) OneWire32(32);
        assert(step==(failAt<9?failAt:8));
        assert(txPending==(failAt==9));
        bus->~OneWire32();
        assert(!bytes.allocated&&!copy.allocated&&!rx.allocated&&!tx.allocated&&!queue.allocated);
    }
}
