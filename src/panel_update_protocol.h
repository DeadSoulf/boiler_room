#pragma once
// Shared, portable update policy. No network, Flash, NVS, clock or UI access.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace panel_update { namespace policy {
constexpr const char* PROJECT="DeadSoulf/boiler_room";
constexpr const char* BOARD="jc4880p443";
constexpr const char* CHIP="esp32p4";
constexpr const char* LAYOUT="guition-16mb-ota-640000-v1";
constexpr const char* LATEST_URL="https://api.github.com/repos/DeadSoulf/boiler_room/releases/latest";
constexpr const char* MANIFEST_NAME="boiler-panel-ota.json";
constexpr uint32_t FLASH_BYTES=0x1000000, SLOT_BYTES=0x640000;
constexpr uint32_t SLOT0=0x10000, SLOT1=0x650000;
constexpr uint16_t CHIP_ID=18, CHIP_REVISION=103;
constexpr uint32_t MIN_FIRMWARE_BYTES=65536;
constexpr size_t MAX_API_BYTES=65536, MAX_MANIFEST_BYTES=4096, MAX_URL=2048;
constexpr uint32_t HEALTHY_MS=10000, HEARTBEAT_MAX_GAP_MS=2000, BOOT_DEADLINE_MS=60000;
constexpr uint32_t CHECK_INTERVAL_MS=6U*60U*60U*1000U;

template<size_t N> inline bool terminated(const char (&s)[N]) {return memchr(s,0,N)!=nullptr;}
inline bool textEquals(const char* a,const char* b) {return a&&b&&!strcmp(a,b);}
template<size_t N> inline bool copy(char (&out)[N],const char* s) {
  if(!s)return false;
  const size_t n=strnlen(s,N);
  if(n>=N)return false;
  memcpy(out,s,n+1);return true;
}
inline bool hex(const char* s,size_t n) {
  if(!s||strnlen(s,n+1)!=n)return false;
  for(size_t i=0;i<n;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
  return true;
}
inline bool decodeHash(const char* s,uint8_t (&out)[32]) {
  if(!hex(s,64))return false;
  for(unsigned i=0;i<32;i++){
    const unsigned a=s[2*i]<='9'?unsigned(s[2*i]-'0'):unsigned(s[2*i]-'a'+10);
    const unsigned b=s[2*i+1]<='9'?unsigned(s[2*i+1]-'0'):unsigned(s[2*i+1]-'a'+10);
    out[i]=uint8_t((a<<4)|b);
  }
  return true;
}
inline bool sameHash(const uint8_t* a,const uint8_t* b) {
  if(!a||!b)return false;
  uint8_t diff=0;for(unsigned i=0;i<32;i++)diff|=uint8_t(a[i]^b[i]);return diff==0;
}
struct Version {uint32_t major=0,minor=0,patch=0;};
inline bool version(const char* s,Version& out) {
  if(!s||s[0]!='v'||strnlen(s,24)>=24)return false;
  Version v;uint32_t* values[]={&v.major,&v.minor,&v.patch};const char* p=s+1;
  for(unsigned i=0;i<3;i++){
    if(*p<'0'||*p>'9')return false;
    const bool leadingZero=*p=='0';unsigned digits=0;uint32_t n=0;
    while(*p>='0'&&*p<='9'){
      const unsigned d=unsigned(*p++-'0');
      if(n>(UINT32_MAX-d)/10U)return false;
      n=n*10U+d;++digits;
    }
    if(leadingZero&&digits!=1)return false;
    *values[i]=n;
    if(i<2){if(*p++!='.')return false;}else if(*p)return false;
  }
  out=v;return true;
}
inline int compare(const Version& a,const Version& b) {
  if(a.major!=b.major)return a.major>b.major?1:-1;
  if(a.minor!=b.minor)return a.minor>b.minor?1:-1;
  if(a.patch!=b.patch)return a.patch>b.patch?1:-1;
  return 0;
}
inline bool newer(const char* candidate,const char* current) {
  Version a,b;return version(candidate,a)&&version(current,b)&&compare(a,b)>0;
}
inline bool firmwareName(char* out,size_t cap,const char* tag) {
  Version v;if(!out||!cap||!version(tag,v))return false;
  const int n=snprintf(out,cap,"BoilerPanel-%s-firmware.bin",tag);
  return n>0&&size_t(n)<cap;
}
inline bool assetUrl(char* out,size_t cap,const char* tag,const char* name) {
  Version v;if(!out||!cap||!name||!version(tag,v))return false;
  char expected[80];if(!firmwareName(expected,sizeof(expected),tag))return false;
  if(strcmp(name,MANIFEST_NAME)&&strcmp(name,expected))return false;
  const int n=snprintf(out,cap,"https://github.com/%s/releases/download/%s/%s",PROJECT,tag,name);
  return n>0&&size_t(n)<cap;
}

enum class Resource:uint8_t {Api,Asset};
inline bool allowedUrl(const char* url,Resource kind,const char* original=nullptr) {
  if(!url)return false;
  const size_t n=strnlen(url,MAX_URL);
  if(n<9||n>=MAX_URL||strncmp(url,"https://",8))return false;
  for(size_t i=0;i<n;i++){
    const unsigned char c=static_cast<unsigned char>(url[i]);
    if(c<=32||c>=127||c=='\\'||c=='#'||c=='"'||c=='<'||c=='>')return false;
  }
  const char* authority=url+8;const char* path=strchr(authority,'/');
  if(!path||path==authority||size_t(path-authority)>80)return false;
  char host[81]={};memcpy(host,authority,size_t(path-authority));
  const char* colon=strchr(host,':');
  if(colon){if(strcmp(colon,":443"))return false;host[colon-host]=0;}
  if(strchr(host,'@')||strchr(host,'?')||strchr(host,':'))return false;
  if(kind==Resource::Api)return textEquals(url,LATEST_URL);
  if(!strcmp(host,"github.com"))return original&&textEquals(url,original);
  if(strcmp(host,"release-assets.githubusercontent.com")&&
     strcmp(host,"objects.githubusercontent.com")&&
     strcmp(host,"github-releases.githubusercontent.com"))return false;
  constexpr const char* prefix="/github-production-release-asset/";
  if(strncmp(path,prefix,strlen(prefix)))return false;
  const char* query=strchr(path,'?');const char* end=query?query:url+n;
  if(end<=path+strlen(prefix))return false;
  // CDN asset paths need no decoding. Reject ambiguous dot/empty/encoded segments.
  for(const char* p=path;p<end;p++){
    const char c=*p;
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.'))return false;
    if(c=='/'&&p+1<end&&p[1]=='/')return false;
    if(c=='/'&&p+1<end&&p[1]=='.'&&(p+2==end||p[2]=='/'||(p[2]=='.'&&(p+3==end||p[3]=='/'))))return false;
  }
  if(end[-1]=='/')return false;
  // Query strings carry GitHub's short-lived signatures; never log or reuse them.
  for(const char* p=query?query+1:end;p<url+n;p++)if(*p=='%'){
    if(p+2>=url+n)return false;
    const auto isHex=[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');};
    if(!isHex(p[1])||!isHex(p[2]))return false;
    if(p[1]=='0'&&(p[2]=='a'||p[2]=='A'||p[2]=='d'||p[2]=='D'||p[2]=='0'))return false;
    p+=2;
  }
  return true;
}

struct Manifest {
  uint32_t schema=0,revisionMin=0,revisionMax=0,flashBytes=0,size=0;
  char project[40]={},board[24]={},chip[16]={},layout[48]={};
  char version[24]={},source[41]={},name[80]={},url[256]={},sha256[65]={};
};
inline bool validManifest(const Manifest& m,const char* tag) {
  if(!terminated(m.project)||!terminated(m.board)||!terminated(m.chip)||!terminated(m.layout)||
     !terminated(m.version)||!terminated(m.source)||!terminated(m.name)||!terminated(m.url)||!terminated(m.sha256))return false;
  char name[80],url[256];Version v;
  return m.schema==1&&textEquals(m.project,PROJECT)&&textEquals(m.board,BOARD)&&
    textEquals(m.chip,CHIP)&&textEquals(m.layout,LAYOUT)&&m.flashBytes==FLASH_BYTES&&
    m.revisionMin==CHIP_REVISION&&m.revisionMax==CHIP_REVISION&&
    version(m.version,v)&&textEquals(m.version,tag)&&hex(m.source,40)&&hex(m.sha256,64)&&
    m.size>=MIN_FIRMWARE_BYTES&&m.size<=SLOT_BYTES&&
    firmwareName(name,sizeof(name),tag)&&textEquals(m.name,name)&&
    assetUrl(url,sizeof(url),tag,name)&&textEquals(m.url,url);
}

// Small bounded JSON reader: it parses the complete document, rejects duplicate
// policy fields, nesting bombs, truncated strings and non-integral size values.
// Only skipped descriptions may contain escaped/Unicode text; identity fields
// must be the literal ASCII form emitted by the release publisher.
class Json {
  const char* p;const char* end;
  static bool digit(char c){return c>='0'&&c<='9';}
  static bool hexDigit(char c){return digit(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F');}
public:
  Json(const char* text,size_t n):p(text),end(text?text+n:text){}
  void ws(){while(p&&p<end&&(*p==' '||*p=='\r'||*p=='\n'||*p=='\t'))++p;}
  bool take(char c){ws();if(!p||p>=end||*p!=c)return false;++p;return true;}
  char peek(){ws();return p&&p<end?*p:0;}
  bool done(){ws();return p&&p==end;}
  bool string(char* out=nullptr,size_t cap=0,bool literal=false){
    if(!take('"'))return false;
    size_t n=0;
    while(p<end){
      const unsigned char c=static_cast<unsigned char>(*p++);
      if(c=='"'){if(out){if(n>=cap)return false;out[n]=0;}return true;}
      if(c<32)return false;
      if(c=='\\'){
        if(literal||p>=end)return false;
        const char e=*p++;
        if(e=='u'){for(unsigned i=0;i<4;i++)if(p>=end||!hexDigit(*p++))return false;}
        else if(!e||!strchr("\"\\/bfnrt",e))return false;
        if(out)return false; // Escaped policy keys/values are not canonical.
      }else if(out){if(c>=127||n+1>=cap)return false;out[n++]=char(c);}
    }
    return false;
  }
  bool uintValue(uint32_t& value){
    ws();if(!p||p>=end||!digit(*p))return false;uint32_t n=0;unsigned digits=0;bool zero=*p=='0';
    while(p<end&&digit(*p)){
      const unsigned d=unsigned(*p++-'0');if(n>(UINT32_MAX-d)/10U)return false;n=n*10U+d;++digits;
    }
    if(zero&&digits>1)return false;
    if(p<end&&(*p=='.'||*p=='e'||*p=='E'))return false;
    value=n;return true;
  }
  bool boolean(bool& value){
    ws();if(p&&end-p>=4&&!memcmp(p,"true",4)){p+=4;value=true;return true;}
    if(p&&end-p>=5&&!memcmp(p,"false",5)){p+=5;value=false;return true;}return false;
  }
  bool skip(unsigned depth=0){
    if(depth>12)return false;
    const char c=peek();
    if(c=='"')return string();
    if(c=='{'||c=='['){
      ++p;const char close=c=='{'?'}':']';if(peek()==close){++p;return true;}
      for(;;){
        if(c=='{'&&(!string()||!take(':')))return false;
        if(!skip(depth+1))return false;
        if(take(close))return true;
        if(!take(','))return false;
      }
    }
    bool unused=false;if(c=='t'||c=='f')return boolean(unused);
    if(c=='n'&&end-p>=4&&!memcmp(p,"null",4)){p+=4;return true;}
    if(c=='-')++p;
    if(p>=end||!digit(*p))return false;
    if(*p=='0')++p;else while(p<end&&digit(*p))++p;
    if(p<end&&*p=='.'){++p;if(p>=end||!digit(*p))return false;while(p<end&&digit(*p))++p;}
    if(p<end&&(*p=='e'||*p=='E')){++p;if(p<end&&(*p=='+'||*p=='-'))++p;if(p>=end||!digit(*p))return false;while(p<end&&digit(*p))++p;}
    return true;
  }
};
inline bool unique(uint32_t& seen,unsigned bit){const uint32_t flag=1U<<bit;if(seen&flag)return false;seen|=flag;return true;}
inline bool firmwareObject(Json& j,Manifest& m){
  if(!j.take('{')||j.peek()=='}')return false;
  uint32_t seen=0;
  for(;;){
    char key[64];if(!j.string(key,sizeof(key),true)||!j.take(':'))return false;
    bool ok=false;
    if(!strcmp(key,"name"))ok=unique(seen,0)&&j.string(m.name,sizeof(m.name),true);
    else if(!strcmp(key,"size"))ok=unique(seen,1)&&j.uintValue(m.size);
    else if(!strcmp(key,"sha256"))ok=unique(seen,2)&&j.string(m.sha256,sizeof(m.sha256),true);
    else if(!strcmp(key,"url"))ok=unique(seen,3)&&j.string(m.url,sizeof(m.url),true);
    else ok=j.skip();
    if(!ok)return false;
    if(j.take('}'))return seen==15U;
    if(!j.take(','))return false;
  }
}
inline bool parseManifest(const char* text,size_t size,const char* tag,Manifest& out){
  if(!text||!size||size>MAX_MANIFEST_BYTES)return false;
  Json j(text,size);Manifest m;uint32_t seen=0;
  if(!j.take('{')||j.peek()=='}')return false;
  for(;;){
    char key[64];if(!j.string(key,sizeof(key),true)||!j.take(':'))return false;bool ok=false;
    if(!strcmp(key,"schema"))ok=unique(seen,0)&&j.uintValue(m.schema);
    else if(!strcmp(key,"project"))ok=unique(seen,1)&&j.string(m.project,sizeof(m.project),true);
    else if(!strcmp(key,"board"))ok=unique(seen,2)&&j.string(m.board,sizeof(m.board),true);
    else if(!strcmp(key,"chip"))ok=unique(seen,3)&&j.string(m.chip,sizeof(m.chip),true);
    else if(!strcmp(key,"chip_revision_min"))ok=unique(seen,4)&&j.uintValue(m.revisionMin);
    else if(!strcmp(key,"chip_revision_max"))ok=unique(seen,5)&&j.uintValue(m.revisionMax);
    else if(!strcmp(key,"flash_bytes"))ok=unique(seen,6)&&j.uintValue(m.flashBytes);
    else if(!strcmp(key,"partition_layout"))ok=unique(seen,7)&&j.string(m.layout,sizeof(m.layout),true);
    else if(!strcmp(key,"version"))ok=unique(seen,8)&&j.string(m.version,sizeof(m.version),true);
    else if(!strcmp(key,"source_commit"))ok=unique(seen,9)&&j.string(m.source,sizeof(m.source),true);
    else if(!strcmp(key,"firmware"))ok=unique(seen,10)&&firmwareObject(j,m);
    else ok=j.skip();
    if(!ok)return false;
    if(j.take('}'))break;
    if(!j.take(','))return false;
  }
  if(seen!=2047U||!j.done()||!validManifest(m,tag))return false;
  out=m;return true;
}

struct Release {
  char version[24]={},manifestUrl[256]={},firmwareUrl[256]={};
  uint32_t manifestSize=0,firmwareSize=0;
};
struct ReleaseAsset {char name[80]={},url[256]={};uint32_t size=0;};
inline bool releaseAsset(Json& j,ReleaseAsset& out){
  if(!j.take('{')||j.peek()=='}')return false;
  ReleaseAsset a;uint32_t seen=0;
  for(;;){
    char key[64];if(!j.string(key,sizeof(key),true)||!j.take(':'))return false;bool ok=false;
    if(!strcmp(key,"name"))ok=unique(seen,0)&&j.string(a.name,sizeof(a.name),true);
    else if(!strcmp(key,"size"))ok=unique(seen,1)&&j.uintValue(a.size);
    else if(!strcmp(key,"browser_download_url"))ok=unique(seen,2)&&j.string(a.url,sizeof(a.url),true);
    else ok=j.skip();
    if(!ok)return false;
    if(j.take('}'))break;
    if(!j.take(','))return false;
  }
  if(seen!=7U)return false;
  out=a;return true;
}
inline bool parseRelease(const char* text,size_t size,Release& out){
  if(!text||!size||size>MAX_API_BYTES)return false;
  Json j(text,size);Release r;ReleaseAsset assets[24];unsigned count=0;
  uint32_t seen=0;bool draft=true,prerelease=true;
  if(!j.take('{')||j.peek()=='}')return false;
  for(;;){
    char key[64];if(!j.string(key,sizeof(key),true)||!j.take(':'))return false;bool ok=false;
    if(!strcmp(key,"tag_name"))ok=unique(seen,0)&&j.string(r.version,sizeof(r.version),true);
    else if(!strcmp(key,"draft"))ok=unique(seen,1)&&j.boolean(draft);
    else if(!strcmp(key,"prerelease"))ok=unique(seen,2)&&j.boolean(prerelease);
    else if(!strcmp(key,"assets")){
      if(!unique(seen,3)||!j.take('['))return false;
      if(j.take(']'))ok=true;
      else for(;;){
        if(count>=24||!releaseAsset(j,assets[count++]))return false;
        if(j.take(']')){ok=true;break;}if(!j.take(','))return false;
      }
    }else ok=j.skip();
    if(!ok)return false;
    if(j.take('}'))break;
    if(!j.take(','))return false;
  }
  Version v;if(seen!=15U||!j.done()||draft||prerelease||!version(r.version,v))return false;
  char name[80],manifestUrl[256],firmwareUrl[256];
  if(!firmwareName(name,sizeof(name),r.version)||
     !assetUrl(manifestUrl,sizeof(manifestUrl),r.version,MANIFEST_NAME)||
     !assetUrl(firmwareUrl,sizeof(firmwareUrl),r.version,name))return false;
  bool foundManifest=false,foundFirmware=false;
  for(unsigned i=0;i<count;i++){
    if(!strcmp(assets[i].name,MANIFEST_NAME)){
      if(foundManifest||strcmp(assets[i].url,manifestUrl)||!assets[i].size||assets[i].size>MAX_MANIFEST_BYTES)return false;
      foundManifest=true;copy(r.manifestUrl,assets[i].url);r.manifestSize=assets[i].size;
    }
    if(!strcmp(assets[i].name,name)){
      if(foundFirmware||strcmp(assets[i].url,firmwareUrl)||assets[i].size<MIN_FIRMWARE_BYTES||assets[i].size>SLOT_BYTES)return false;
      foundFirmware=true;copy(r.firmwareUrl,assets[i].url);r.firmwareSize=assets[i].size;
    }
  }
  if(!foundManifest||!foundFirmware)return false;
  out=r;return true;
}

struct Hardware {uint32_t flashBytes=0,slot0=0,slot1=0,slotBytes=0;uint16_t chipId=0,revision=0;};
inline bool validHardware(const Hardware& h){
  return h.flashBytes==FLASH_BYTES&&h.slot0==SLOT0&&h.slot1==SLOT1&&h.slotBytes==SLOT_BYTES&&h.chipId==CHIP_ID&&h.revision==CHIP_REVISION;
}
inline uint16_t read16(const uint8_t* p){return uint16_t(p[0])|uint16_t(uint16_t(p[1])<<8);}
inline uint32_t read32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline bool validPartitionTable(const uint8_t* data,size_t size){
  if(!data||size!=4096)return false;
  struct Entry {uint8_t type,subtype;uint32_t address,size;const char* label;};
  const Entry expected[]={
    {1,2,0x9000,0x5000,"nvs"},{1,0,0xE000,0x2000,"otadata"},
    {0,0x10,SLOT0,SLOT_BYTES,"app0"},{0,0x11,SLOT1,SLOT_BYTES,"app1"},
    {1,0x82,0xC90000,0x360000,"spiffs"},{1,3,0xFF0000,0x10000,"coredump"}
  };
  for(unsigned i=0;i<6;i++){
    const uint8_t* p=data+32*i;const Entry& e=expected[i];const size_t length=strlen(e.label);
    if(read16(p)!=0x50AA||p[2]!=e.type||p[3]!=e.subtype||read32(p+4)!=e.address||
       read32(p+8)!=e.size||read32(p+28)!=0||memcmp(p+12,e.label,length))return false;
    for(size_t n=length;n<16;n++)if(p[12+n]!=0)return false;
  }
  // MD5 of the exact 192 canonical entry bytes above; this is a layout checksum,
  // not firmware authentication. Also reject additional partition records/data.
  const uint8_t expectedMd5[]={0xb3,0x2d,0xcd,0xb8,0x3b,0x16,0x74,0xaf,0x27,0x76,0x8c,0x92,0xb1,0xf8,0x8a,0x62};
  if(read16(data+192)!=0xEBEB)return false;
  for(unsigned i=194;i<208;i++)if(data[i]!=255)return false;
  if(memcmp(data+208,expectedMd5,sizeof(expectedMd5)))return false;
  for(size_t i=224;i<size;i++)if(data[i]!=255)return false;
  return true;
}
inline bool validImagePrefix(const uint8_t* data,size_t size,uint16_t revision=CHIP_REVISION){
  // esp_image_header_t (24), first segment header (8), esp_app_desc_t magic.
  if(!data||size<36||data[0]!=0xE9||data[1]==0||data[1]>16)return false;
  if(read16(data+12)!=CHIP_ID||data[23]!=1||read32(data+32)!=0xABCD5432U||read32(data+28)<256)return false;
  if((data[3]>>4)!=4)return false; // Header declares 16 MiB Flash, not another board layout.
  const uint16_t minimum=read16(data+15),maximum=read16(data+17);
  return minimum<=revision&&(maximum==0||maximum==65535||revision<=maximum)&&data[14]<=revision/100;
}

// The runtime uses this gate before esp_ota_end and again before boot selection.
// It is intentionally separate from transport errors and UI state.
class TransferGate {
  uint32_t expected_,received_=0;bool header_=false,hash_=false,ended_=false,failed_=false,cancelled_=false;
public:
  explicit TransferGate(uint32_t expected):expected_(expected){if(expected<MIN_FIRMWARE_BYTES||expected>SLOT_BYTES)failed_=true;}
  bool add(uint32_t n){if(failed_||cancelled_||ended_||n>expected_-received_){failed_=true;return false;}received_+=n;return true;}
  void header(bool valid){header_=valid;if(!valid)failed_=true;}
  void digest(const uint8_t* actual,const uint8_t* expected){hash_=sameHash(actual,expected);if(!hash_)failed_=true;}
  void cancel(){cancelled_=true;}
  void fail(){failed_=true;}
  bool canEnd()const{return !failed_&&!cancelled_&&header_&&hash_&&received_==expected_;}
  void end(bool valid){if(!canEnd()||!valid)failed_=true;else ended_=true;}
  bool canSelect()const{return ended_&&!failed_&&!cancelled_&&header_&&hash_&&received_==expected_;}
  uint32_t received()const{return received_;}
};

enum class BootObservation:uint8_t {Unrelated,Interrupted,Pending,Confirmed,RolledBack,Mismatch};
inline BootObservation observeBoot(uint32_t running,const char* actual,uint32_t from,const char* previous,
                                  uint32_t target,const char* next,bool armed,bool confirmed,bool pending,bool targetAborted){
  if(!armed&&!confirmed)return BootObservation::Unrelated;
  if(running==target){
    if(!textEquals(actual,next))return BootObservation::Mismatch;
    if(pending)return BootObservation::Pending;
    return confirmed?BootObservation::Confirmed:BootObservation::Mismatch;
  }
  if(running==from&&textEquals(actual,previous))return targetAborted?BootObservation::RolledBack:BootObservation::Interrupted;
  return BootObservation::Mismatch;
}
}} // namespace panel_update::policy
