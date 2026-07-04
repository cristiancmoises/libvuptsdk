/* ML-KEM-768 ACVP Known-Answer-Test driver for libvuptsdk (FIPS 203).
 *
 * Overrides the library's zupt_random_bytes with a seed-injecting stub so
 * keygen/encaps run deterministically on the vectors' (d,z)/m. Prints "1" for
 * a bit-exact match, "0" otherwise. Wire this into CI as a BLOCKING gate.
 *
 * Build (from a libvuptsdk checkout):
 *   gcc -O2 -Iinclude -Isrc kat_mlkem768_acvp.c \
 *       src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c -o katz
 * Modes:
 *   ./katz keygen     <d> <z> <ek> <dk>
 *   ./katz encap      <ek> <m> <c> <k>
 *   ./katz decap      <dk> <c> <k>
 *   ./katz encapCheck <expected 0|1> <ek>     (FIPS 203 §7.2 modulus check)
 *   ./katz decapCheck <expected 0|1> <dk>     (FIPS 203 §7.3 hash check)
 *
 * Exit status: 0 on a well-formed invocation, 2 on a usage error (missing
 * args / unknown mode) so a mistyped CI step can never masquerade as a pass.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int zupt_mlkem768_keygen(uint8_t pk[1184], uint8_t sk[2400]);
int zupt_mlkem768_encaps(uint8_t ct[1088], uint8_t ss[32], const uint8_t pk[1184]);
int zupt_mlkem768_decaps(uint8_t ss[32], const uint8_t ct[1088], const uint8_t sk[2400]);
int zupt_mlkem768_check_ek(const uint8_t *ek, size_t len);
int zupt_mlkem768_check_dk(const uint8_t *dk, size_t len);

static uint8_t inj[512]; static size_t inj_len=0, inj_pos=0;
void zupt_random_bytes(uint8_t *buf, size_t len){
    for(size_t i=0;i<len;i++) buf[i]=(inj_pos<inj_len)?inj[inj_pos++]:0;
}
static void feed(const uint8_t*p,size_t n){memcpy(inj+inj_len,p,n);inj_len+=n;}
static void reset(void){inj_len=inj_pos=0;}
static int hx(const char*h,uint8_t*o,size_t max){size_t n=strlen(h)/2;if(n>max)return -1;
    for(size_t i=0;i<n;i++)sscanf(h+2*i,"%2hhx",&o[i]);return (int)n;}
static int eq(const uint8_t*a,const uint8_t*b,size_t n){return memcmp(a,b,n)==0;}
static void need(int argc,int n){ if(argc<n){fprintf(stderr,"katz: missing arguments\n");exit(2);} }

int main(int argc,char**argv){
    static uint8_t a[8192],b[8192];
    if(argc<2){fprintf(stderr,"usage: katz keygen|encap|decap|encapCheck|decapCheck ...\n");return 2;}
    if(!strcmp(argv[1],"keygen")){
        need(argc,6);
        uint8_t dd[32],zz[32],ek[1184],dk[2400];
        hx(argv[2],dd,32);hx(argv[3],zz,32);int nek=hx(argv[4],a,sizeof(a)),ndk=hx(argv[5],b,sizeof(b));
        reset();feed(dd,32);feed(zz,32);
        zupt_mlkem768_keygen(ek,dk);
        printf("%d\n",(nek==1184&&ndk==2400&&eq(ek,a,1184)&&eq(dk,b,2400))?1:0);
    } else if(!strcmp(argv[1],"encap")){
        need(argc,6);
        uint8_t ek[1184],mm[32],ct[1088],ss[32];
        hx(argv[2],ek,1184);hx(argv[3],mm,32);int nc=hx(argv[4],a,sizeof(a)),nk=hx(argv[5],b,sizeof(b));
        reset();feed(mm,32);
        zupt_mlkem768_encaps(ct,ss,ek);
        printf("%d\n",(nc==1088&&nk==32&&eq(ct,a,1088)&&eq(ss,b,32))?1:0);
    } else if(!strcmp(argv[1],"decap")){
        need(argc,5);
        uint8_t dk[2400],ct[1088],ss[32];
        hx(argv[2],dk,2400);hx(argv[3],ct,1088);int nk=hx(argv[4],b,sizeof(b));
        zupt_mlkem768_decaps(ss,ct,dk);
        printf("%d\n",(nk==32&&eq(ss,b,32))?1:0);
    } else if(!strcmp(argv[1],"encapCheck")){
        need(argc,4);
        int expect=atoi(argv[2]);
        int n=hx(argv[3],a,sizeof(a));
        int valid=(n>=0 && zupt_mlkem768_check_ek(a,(size_t)n)==0)?1:0;
        printf("%d\n",(valid==expect)?1:0);
    } else if(!strcmp(argv[1],"decapCheck")){
        need(argc,4);
        int expect=atoi(argv[2]);
        int n=hx(argv[3],a,sizeof(a));
        int valid=(n>=0 && zupt_mlkem768_check_dk(a,(size_t)n)==0)?1:0;
        printf("%d\n",(valid==expect)?1:0);
    } else {
        fprintf(stderr,"katz: unknown mode '%s'\n",argv[1]);
        return 2;
    }
    return 0;
}
