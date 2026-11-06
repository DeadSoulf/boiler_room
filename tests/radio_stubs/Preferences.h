#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <cstring>
class Preferences {public:
 std::string lastNamespace,lastKey;bool opens=true;
 bool writes=true,autoValue=false,credentialKey=false;unsigned credentialWrites=0,displayWrites=0;uint32_t displayValue=0;
 std::vector<unsigned char> credentialBytes;bool reads=true;unsigned credentialReads=0;
 bool begin(const char* name,bool){lastNamespace=name;return opens;}
 unsigned char getUChar(const char*,unsigned char x){return x;}
 uint32_t getUInt(const char* key,uint32_t){lastKey=key;return displayValue;}
 size_t putUInt(const char* key,uint32_t v){lastKey=key;++displayWrites;if(!writes)return 0;displayValue=v;return sizeof(v);}
 size_t getBytesLength(const char*){return credentialKey?credentialBytes.size():0;}
 size_t getBytes(const char*,void* out,size_t size){++credentialReads;if(!reads||size!=credentialBytes.size())return 0;std::memcpy(out,credentialBytes.data(),size);return size;}
 bool getBool(const char*,bool){return autoValue;}
 size_t putBytes(const char*,const void* data,size_t n){++credentialWrites;if(writes){credentialKey=true;const auto* bytes=static_cast<const unsigned char*>(data);credentialBytes.assign(bytes,bytes+n);}return writes?n:0;}
 size_t putBool(const char*,bool v){if(writes)autoValue=v;return writes?1:0;}
 size_t putUChar(const char*,unsigned char){return writes?1:0;}
 bool isKey(const char*){return credentialKey;}
 bool remove(const char*){if(writes)credentialKey=false;return writes;}
};
