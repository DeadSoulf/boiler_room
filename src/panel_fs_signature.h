#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Read-only volume signature recognition. This is NOT an exFAT file driver.
// Physical sector access is performed by the dedicated SD worker only.
namespace panel_fs_signature {
enum class Type : uint8_t { Unknown=0, Fat=1, ExFat=2, Other=3 };
inline uint32_t le32(const uint8_t* p){
  return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
}
inline uint64_t le64(const uint8_t* p){
  return uint64_t(le32(p))|(uint64_t(le32(p+4))<<32);
}
inline Type bootSector(const uint8_t* b,size_t length){
  if(!b||length<512)return Type::Unknown;
  // exFAT filesystem name is exactly 8 bytes at byte offset 3 of the VBR.
  if(memcmp(b+3,"EXFAT   ",8)==0 && b[510]==0x55 && b[511]==0xAA)return Type::ExFat;
  if(b[510]!=0x55 || b[511]!=0xAA)return Type::Unknown;
  if(memcmp(b+82,"FAT32   ",8)==0 || memcmp(b+54,"FAT16   ",8)==0 ||
     memcmp(b+54,"FAT12   ",8)==0)return Type::Fat;
  return Type::Other;
}
inline uint32_t mbrFirstPartition(const uint8_t* b,size_t length){
  if(!b||length<512||b[510]!=0x55||b[511]!=0xAA)return 0;
  for(unsigned i=0;i<4;i++){
    const uint8_t* entry=b+446+i*16;
    const uint8_t type=entry[4];
    const uint32_t start=le32(entry+8),count=le32(entry+12);
    // MBR partition type 0xee means GPT protective MBR, handled separately.
    if(type && count && start && type!=0xEE)return start;
  }
  return 0;
}
inline bool protectiveGpt(const uint8_t* b,size_t length){
  if(!b||length<512||b[510]!=0x55||b[511]!=0xAA)return false;
  for(unsigned i=0;i<4;i++)if(b[446+i*16+4]==0xEE)return true;
  return false;
}
} // namespace panel_fs_signature
