#include "panel_fs_probe.h"
#include <Arduino.h>
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_err.h"
#include <string.h>

namespace panel_fs_probe {
namespace {
bool read(const sdmmc_card_t* card,uint32_t sector,uint8_t (&data)[512]){
  // The IDF driver API accepts a mutable sdmmc_card_t.
  return sdmmc_read_sectors(const_cast<sdmmc_card_t*>(card),data,sector,1)==ESP_OK;
}
void slot0Pins(sdmmc_slot_config_t& slot){
  // ESP32-P4 Slot 0 fixed IO_MUX pins; same configuration as the board's
  // existing format-only SDMMC path. Do not touch GPIO45/LDO4.
  slot.clk=GPIO_NUM_0;slot.cmd=GPIO_NUM_0;
  slot.d0=GPIO_NUM_0;slot.d1=GPIO_NUM_0;slot.d2=GPIO_NUM_0;slot.d3=GPIO_NUM_0;
  slot.d4=GPIO_NUM_0;slot.d5=GPIO_NUM_0;slot.d6=GPIO_NUM_0;slot.d7=GPIO_NUM_0;
  slot.cd=SDMMC_SLOT_NO_CD;slot.wp=SDMMC_SLOT_NO_WP;
  slot.width=4;slot.flags=0;
}
}
Result inspect(){
  Result out;
  sdmmc_host_t host=SDMMC_HOST_DEFAULT();
  host.slot=SDMMC_HOST_SLOT_0;
  host.flags=SDMMC_HOST_FLAG_4BIT;
  host.max_freq_khz=SDMMC_FREQ_DEFAULT;
  sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT();
  slot0Pins(slot);
  if(sdmmc_host_init()!=ESP_OK)return out;
  if(sdmmc_host_init_slot(host.slot,&slot)!=ESP_OK){
    sdmmc_host_deinit();return out;
  }
  sdmmc_card_t card={};
  if(sdmmc_card_init(&host,&card)!=ESP_OK){
    sdmmc_host_deinit();return out;
  }
  out.cardPresent=true;
  out.cardBytes=uint64_t(card.csd.capacity)*uint64_t(card.csd.sector_size);
  alignas(4) uint8_t sector[512]={};
  if(read(&card,0,sector)){
    out.sectorReadable=true;
    out.type=panel_fs_signature::bootSector(sector,512);
    // Typical cards use MBR partition tables; some use GPT.
    const uint32_t first=panel_fs_signature::mbrFirstPartition(sector,512);
    const bool gpt=panel_fs_signature::protectiveGpt(sector,512);
    if(first && read(&card,first,sector))
      out.type=panel_fs_signature::bootSector(sector,512);
    else if(gpt && read(&card,1,sector) &&
            memcmp(sector,"EFI PART",8)==0){
      const uint64_t entries=panel_fs_signature::le64(sector+72);
      if(entries && entries<=UINT32_MAX && read(&card,uint32_t(entries),sector)){
        const uint64_t lba=panel_fs_signature::le64(sector+32);
        if(lba && lba<=UINT32_MAX && read(&card,uint32_t(lba),sector))
          out.type=panel_fs_signature::bootSector(sector,512);
      }
    }
  }
  // Never write, format, or mount a volume from the probe path.
  sdmmc_host_deinit();
  return out;
}
} // namespace panel_fs_probe
