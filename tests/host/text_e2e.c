#include "saan_kanji.h"
#include "demo_ids.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static unsigned char arena[180224] __attribute__((aligned(16)));
static int32_t ids[350];
static void same_ids(const jdict_t *d, const char *a, const char *b, int phonemes_only) {
 int32_t left[350], right[350], nl=0, nr=0; int nt=0;
 assert(saan_kanji_to_ids(d,a,strlen(a),arena,sizeof(arena),left,350,&nl,&nt)==0);
 assert(saan_kanji_to_ids(d,b,strlen(b),arena,sizeof(arena),right,350,&nr,&nt)==0);
 if (phonemes_only) {
  const char *symbols[]={"^","$","_","[","]","#","?","?!","?.","?~"};
  int32_t *arrays[]={left,right}; int32_t *sizes[]={&nl,&nr};
  for(int a=0;a<2;a++) { int out=0;
   for(int i=0;i<*sizes[a];i++) { int symbol=0;
    for(unsigned j=0;j<sizeof(symbols)/sizeof(symbols[0]);j++)
     if(arrays[a][i]==label_ids_token_id(symbols[j])) symbol=1;
    if(!symbol) arrays[a][out++]=arrays[a][i];
   }
   *sizes[a]=out;
  }
 }
 fprintf(stderr,"Comparing %s / %s: %d / %d ids\n",a,b,nl,nr);
 assert(nl==nr && !memcmp(left,right,(size_t)nl*sizeof(*left)));
 /* Require an actual phoneme, not merely boundary/pause/accent symbols. */
 int voiced=0;
 const char *phonemes[]={"a","i","u","e","o","k","s","t","n","h","m","y","r","w"};
 for(int i=0;i<nl;i++) for(unsigned j=0;j<sizeof(phonemes)/sizeof(phonemes[0]);j++)
  if(left[i]==label_ids_token_id(phonemes[j])) voiced=1;
 assert(voiced);
 printf("Normalization/reference PASS: %s == %s (%d ids)\n",a,b,nl);
}
int main(int argc,char **argv) {
 FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);
 unsigned char *blob=malloc(len);assert(fread(blob,1,len,f)==(size_t)len);fclose(f);
 jdict_t d;assert(jdict_open(&d,blob,len)==0);
 const char *texts[]={"今日は良い天気ですね。","こんにちは。","東京都に行きます。","123個あります。","スーパーでパンを買います。","OpenAIで開発します。","ぬるぽぴょんテストです。"};
 int32_t first[350],first_n=0;
 for(int repeat=0;repeat<3;repeat++)for(unsigned i=0;i<sizeof(texts)/sizeof(texts[0]);i++) {
  memset(arena,0xa5,sizeof(arena));int32_t n=0;int nt=0;
  int s=saan_kanji_to_ids(&d,texts[i],strlen(texts[i]),arena,sizeof(arena),ids,350,&n,&nt);
  printf("parse round=%d text=%s status=%d tokens=%d ids=%d\n",repeat,texts[i],s,nt,n);
  assert(s==0 && n>0 && n<=350);
  if(i==0) {if(repeat==0){memcpy(first,ids,n*4);first_n=n;}else assert(n==first_n && !memcmp(first,ids,n*4));}
 }
 same_ids(&d,"123個あります。","１２３個あります。",0);
 same_ids(&d,"OpenAIで開発します。","ＯｐｅｎＡＩで開発します。",0);
 same_ids(&d,"123","１２３",0);
 same_ids(&d,"OpenAI","ＯｐｅｎＡＩ",0);
 // A kana reading reference prevents two equally silent spellings from passing.
 same_ids(&d,"OpenAI","オーピーイーエヌエーアイ",1);
 same_ids(&d,"123","ヒャクニジュウサン",1);
 // Limit errors must not prevent the next short input from succeeding.
 char longtext[1024];memset(longtext,'a',342);longtext[342]=0;int32_t n=0;int nt=0;
 int s=saan_kanji_to_ids(&d,longtext,342,arena,sizeof(arena),ids,350,&n,&nt);assert(s==SAAN_KANJI_ERR_TOO_LONG);
 s=saan_kanji_to_ids(&d,texts[0],strlen(texts[0]),arena,sizeof(arena),ids,5,&n,&nt);assert(s!=0);
 s=saan_kanji_to_ids(&d,texts[0],strlen(texts[0]),arena,sizeof(arena),ids,350,&n,&nt);assert(s==0);
 assert(first_n == SAAN_DEMO_N_IDS && !memcmp(first, kSaanDemoIds, sizeof(kSaanDemoIds)));
 free(blob);puts("Japanese text -> ids repeat/limits PASS");return 0;
}
