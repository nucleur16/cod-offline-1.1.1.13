/* Real native archive loader validation; no key or asset contents printed. */
#include "codol_assets.h"
#include "mbedtls/sha256.h"
#include "zlib/zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
voidpf zcalloc(voidpf unused,unsigned count,unsigned size){(void)unused;return size&&count>SIZE_MAX/size?NULL:calloc(count,size);}
void zcfree(voidpf unused,voidpf ptr){(void)unused;free(ptr);}
int main(int argc,char **argv){
 codol_zone_t zone={0};unsigned i,j,rawfiles=0;uint8_t digest[32];codol_script_string_t *strings=NULL;
 if(argc!=3)return 2;
 if(!CODOL_OpenFastFile(&zone,argv[1],argv[2])){puts("{\"accepted\":false,\"stage\":\"native_open_fastfile\"}");return 1;}
 if(!CODOL_ReadScriptStrings(&zone,&strings)){CODOL_CloseZone(&zone);return 3;}free(strings);
 for(i=0;i<zone.asset_count;i++){
  codol_asset_t asset;uint8_t *payload=NULL;size_t length;
  if(!CODOL_ZoneAsset(&zone,i,&asset)){CODOL_CloseZone(&zone);return 4;}
  if(asset.type==CODOL_ASSET_RAWFILE){
   if(!CODOL_ReadRawFile(&asset,&payload,&length)){CODOL_CloseZone(&zone);return 5;}
   free(payload);rawfiles++;
  }
 }
 if(mbedtls_sha256_ret(zone.data,zone.size,digest,0)){CODOL_CloseZone(&zone);return 6;}
 printf("{\"accepted\":true,\"zone_bytes\":%zu,\"assets\":%u,\"strings\":%u,\"rawfiles_validated\":%u,\"zone_sha256\":\"",zone.size,zone.asset_count,zone.string_count,rawfiles);
 for(j=0;j<sizeof(digest);j++)printf("%02x",digest[j]);puts("\"}");
 CODOL_CloseZone(&zone);return 0;
}
