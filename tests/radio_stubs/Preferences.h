#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
class Preferences {public:
 std::string lastNamespace,lastKey;bool opens=true;
 bool writes=true,autoValue=false,credentialKey=false;unsigned credentialWrites=0,displayWrites=0;uint32_t displayValue=0;
 bool begin(const char* name,bool){lastNamespace=name;return opens;}
 unsigned char getUChar(const char*,unsigned char x){return x;}
 uint32_t getUInt(const char* key,uint32_t){lastKey=key;return displayValue;}
 size_t putUInt(const char* key,uint32_t v){lastKey=key;++displayWrites;if(!writes)return 0;displayValue=v;return sizeof(v);}
 size_t getBytesLength(const char*){return 0;}
 size_t getBytes(const char*,void*,size_t){return 0;}
 bool getBool(const char*,bool){return autoValue;}
 size_t putBytes(const char*,const void*,size_t n){++credentialWrites;if(writes)credentialKey=true;return writes?n:0;}
 size_t putBool(const char*,bool v){if(writes)autoValue=v;return writes?1:0;}
 size_t putUChar(const char*,unsigned char){return writes?1:0;}
 bool isKey(const char*){return credentialKey;}
 bool remove(const char*){if(writes)credentialKey=false;return writes;}
};
