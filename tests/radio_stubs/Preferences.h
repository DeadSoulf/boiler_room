#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <cstring>
class Preferences {public:
 std::string lastNamespace,lastKey;bool opens=true;
 bool writes=true,autoValue=false,credentialKey=false;unsigned credentialWrites=0,displayWrites=0;uint32_t displayValue=0;
 // Keep the legacy fixture controls independent from every new blob: a damaged
 // primary must never silently borrow the old credential or another key's bytes.
 std::vector<unsigned char> credentialBytes;std::map<std::string,std::vector<unsigned char>> blobs;
 bool reads=true;unsigned credentialReads=0;std::string failReadKey,failLengthKey,failWriteKey,failRemoveKey;
 bool begin(const char* name,bool){lastNamespace=name;return opens;}
 unsigned char getUChar(const char*,unsigned char x){return x;}
 uint32_t getUInt(const char* key,uint32_t){lastKey=key;return displayValue;}
 size_t putUInt(const char* key,uint32_t v){lastKey=key;++displayWrites;if(!writes)return 0;displayValue=v;return sizeof(v);}
 const std::vector<unsigned char>* blob(const char* key)const{
   if(!std::strcmp(key,"credentials"))return credentialKey?&credentialBytes:nullptr;
   const auto it=blobs.find(key);return it==blobs.end()?nullptr:&it->second;
 }
 size_t getBytesLength(const char* key){if(!reads||failReadKey==key||failLengthKey==key)return 0;const auto* b=blob(key);return b?b->size():0;}
 size_t getBytes(const char* key,void* out,size_t cap){
   ++credentialReads;const auto* b=blob(key);
   if(!reads||failReadKey==key||!b||b->empty()||cap<b->size())return 0;
   std::memcpy(out,b->data(),b->size());return b->size();
 }
 bool getBool(const char*,bool){return autoValue;}
 size_t putBytes(const char* key,const void* data,size_t n){
   ++credentialWrites;if(!writes||failWriteKey==key)return 0;
   const auto* bytes=static_cast<const unsigned char*>(data);
   if(!std::strcmp(key,"credentials")){credentialKey=true;credentialBytes.assign(bytes,bytes+n);}
   else blobs[key].assign(bytes,bytes+n);
   return n;
 }
 size_t putBool(const char* key,bool v){if(!writes||failWriteKey==key)return 0;autoValue=v;return 1;}
 size_t putUChar(const char*,unsigned char){return writes?1:0;}
 bool isKey(const char* key){return reads&&failReadKey!=key&&failLengthKey!=key&&blob(key)!=nullptr;}
 bool remove(const char* key){
   if(!writes||failRemoveKey==key||!isKey(key))return false;
   if(!std::strcmp(key,"credentials")){credentialKey=false;credentialBytes.clear();}else blobs.erase(key);
   return true;
 }
};
