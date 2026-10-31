/* gmb_ai_finetune.c — fine-tune LoRA + apply/deploy IN-PROCESS. Vedi include/gmb_ai_finetune.h.
 *
 * Derivato dalla POC storica del fine-tune LoRA (logica invariata: SPSA + AdamW, loss CE
 * full-vocab via Accelerate, adapter applicato nel forward nativo del motore).
 * Differenze rispetto alla POC:
 *  - `main(argc,argv)` -> funzione con request JSON in ingresso e response JSON in uscita (stesso
 *    contratto del protocollo IPC), così il runtime C e il C# la chiamano senza subprocess.
 *  - NIENTE `exit()` (ucciderebbe l'host): gli errori tornano come response {"error":...}.
 *  - NIENTE leak: libera tutto (la POC si affidava alla morte del processo). */
#include "gmb_ai_finetune.h"
#include "llmengine_mlx.h"
#include "llmengine.h"
#include "llmtoken.h"
#include "train/llm_train.h"   /* llm_ai_mlx_train_dpo_step + llm_ai_mlx_train_kl_step + llm_train_teacher_logits (PUL/gmbllm) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <Accelerate/Accelerate.h>
#include <pthread.h>

/* Crea ricorsivamente la cartella che contiene `path` (es. "work/ft1_best_A.raw" -> mkdir work/).
 * Senza, fopen("work/...","wb") falliva IN SILENZIO (l'`if(f)` salta) e il checkpoint non veniva
 * salvato: il successivo TRAIN LORA APPLY dava "open B". */
static void gmb_mkdirs_for(const char* path) {
    if (!path || !*path) return;
    char buf[1100]; snprintf(buf, sizeof buf, "%s", path);
    for (char* p = buf + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(buf, 0755); *p = '/'; }
    }
}

/* llm_ai_mlx_model_lora_{add,set_enabled,clear} sono dichiarate in llmengine_mlx.h. */

/* Dimensioni del modello: NON più hardcoded. Vengono risolte a runtime da
 * load_model_shape() (config.json + header safetensors), così il fine-tune funziona
 * su architetture arbitrarie. I valori sotto sono solo il fallback storico (Llama-1B). */
static int H = 2048;       /* hidden_size */
static int VOCAB = 128256; /* vocab_size */
static int VIN = 2048;     /* in_dim del modulo LoRA target (v_proj) */
static int VOUT = 512;     /* out_dim del modulo LoRA target (v_proj) */
static int g_wpu = 8;      /* pesi per uint32 nella quantizzazione affine (4-bit=8, 8-bit=4) */
static int g_group = 64;   /* group size affine (da scales.shape) */
static char g_lora_mod[32] = ""; /* modulo LoRA target: ""=auto-rilevato, altrimenti esplicito */
#define DEFAULT_RANK 4
#define DEFAULT_LAYER 15
#define MAXT 64    /* max finestra */

/* ---- errori senza exit(): messaggio + flag, propagati alla response ---- */
static char g_err[256];
static int g_failed;
static const LlmEngineLlmEngine* E = NULL;   /* motore (via ABI llmengine); MLX è l'unico con CAP_LORA */
static void fail(const char* m) { if (!g_failed) { g_failed = 1; snprintf(g_err, sizeof g_err, "%s", m); } }
/* response di ERRORE nel formato del protocollo IPC ({"type":"error","data":{"message":..}}) */
static int err_resp(char* resp, size_t cap, const char* msg) {
    snprintf(resp, cap, "{\"type\":\"error\",\"data\":{\"message\":\"%s\"}}\n", msg);
    return -1;
}

static float* falloc(size_t n) { float* p = (float*)malloc(n * sizeof(float)); if (!p) fail("oom"); return p; }

/* ---- mini-parser JSON piatto: "key":"str" | "key":num (il request è generato da
 * gmb_ipc_build_* e non ha annidamenti nei params) ---- */
static const char* jfind(const char* js, const char* key) {
    char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* q = strstr(js, pat); if (!q) return NULL;
    q = strchr(q + strlen(pat), ':'); if (!q) return NULL;
    return q + 1;
}
static void jstr(const char* js, const char* key, char* out, size_t cap) {
    const char* p = jfind(js, key);
    if (!p || cap == 0) { if (cap) out[0] = 0; return; }
    while (*p == ' ') p++;
    size_t o = 0;
    if (*p == '"') { p++; while (*p && *p != '"' && o + 1 < cap) out[o++] = *p++; }
    else { while (*p && *p != ',' && *p != '}' && *p != ' ' && o + 1 < cap) out[o++] = *p++; }
    out[o] = 0;
}
static long jlong(const char* js, const char* key, long dflt) {
    const char* p = jfind(js, key); if (!p) return dflt; return strtol(p, NULL, 10);
}
static double jdbl(const char* js, const char* key, double dflt) {
    const char* p = jfind(js, key); if (!p) return dflt; return atof(p);
}

/* ---------------- safetensors reader (lm_head = embed, tie) ---------------- */
static int gmb_run_big_stack(int (*fn)(const char*, char*, size_t), const char* req, char* resp, size_t cap);

static uint8_t* g_raw; static char* g_hdr_js;
typedef struct { char dtype[8]; int ndim; int shape[4]; size_t off0, off1; } STMeta;
static float half2f(uint16_t h){
    uint32_t sign=(uint32_t)(h>>15)&1, exp=(uint32_t)(h>>10)&0x1F, man=h&0x3FF, bits;
    if(exp==0){ if(man==0) bits=sign<<31; else { int e=-1; uint32_t m=man;
        while(!(m&0x400)){m<<=1;e++;} m&=0x3FF; bits=(sign<<31)|((uint32_t)(127-15-e)<<23)|(m<<13);} }
    else if(exp==0x1F){ if(man && !(man&0x200u)) man|=0x200u; bits=(sign<<31)|0x7F800000|(man<<13); }
    else bits=(sign<<31)|((exp-15+127)<<23)|(man<<13);
    float f; memcpy(&f,&bits,4); return f;
}
static float bf162f(uint16_t h){ uint32_t bits=(uint32_t)h<<16; float f; memcpy(&f,&bits,4); return f; }
/* legge l'elemento i-esimo di un tensore in base al dtype (F16/BF16/F32). */
static float st_elem(const STMeta* m, size_t i){
    if(strncmp(m->dtype,"BF16",4)==0) return bf162f(((const uint16_t*)g_raw)[m->off0/2+i]);
    if(strncmp(m->dtype,"F32",3)==0)  return ((const float*)g_raw)[m->off0/4+i];
    return half2f(((const uint16_t*)g_raw)[m->off0/2+i]);
}
static int st_open(const char* path){
    FILE* f=fopen(path,"rb"); if(!f){ fail("open safetensors"); return -1; }
    uint64_t n; if(fread(&n,8,1,f)!=1){ fclose(f); fail("hdr len"); return -1; }
    char* js=(char*)malloc(n+1); if(!js || fread(js,1,n,f)!=n){ fclose(f); fail("hdr read"); return -1; } js[n]=0;
    fseek(f,0,SEEK_END); long fsz=ftell(f); fseek(f,8+(long)n,SEEK_SET);
    g_raw=(uint8_t*)malloc((size_t)(fsz-8-n));
    if(!g_raw || fread(g_raw,1,(size_t)(fsz-8-n),f)!=(size_t)(fsz-8-n)){ fclose(f); fail("data read"); return -1; }
    fclose(f); g_hdr_js=js;
    return 0;
}
static void st_close(void){ free(g_raw); g_raw=NULL; free(g_hdr_js); g_hdr_js=NULL; }
static int st_meta(const char* name, STMeta* m){
    char pat[256]; snprintf(pat,sizeof pat,"\"%s\":",name);
    const char* q=strstr(g_hdr_js,pat); if(!q) return -1;
    q=strchr(q,'{'); if(!q) return -1;
    const char* e=strchr(q,'}'); if(!e) return -1;
    char buf[512]; size_t len=(size_t)(e-q); if(len>=sizeof buf) len=sizeof buf-1;
    memcpy(buf,q+1,len); buf[len]=0;
    memset(m,0,sizeof*m);
    const char* shp=strstr(buf,"\"shape\""); if(!shp) return -1;
    shp=strchr(shp,'[');
    { const char* sc=shp+1; int idx=0;
      while(*sc&&*sc!=']'&&idx<4){ if(*sc==' '||*sc==','){sc++;continue;}
        char* end; long vv=strtol(sc,&end,10); if(end==sc) return -1; m->shape[idx++]=(int)vv; sc=end; }
      m->ndim=idx; }
    const char* d=strstr(buf,"\"data_offsets\""); if(!d) return -1;
    d=strchr(d,'[');
    { const char* oc=d+1; char* end;
      unsigned long a=strtoul(oc,&end,10); if(end==oc) return -1;
      while(*end==' '||*end==',') end++;
      unsigned long b=strtoul(end,&end,10); m->off0=a; m->off1=b; }
    const char* dt=strstr(buf,"\"dtype\""); if(dt){
        const char* c1=strchr(dt,':'); if(c1){ c1=strchr(c1+1,'"');
            if(c1){ c1++; int o=0; while(*c1 && *c1!='"' && o<7) m->dtype[o++]=*c1++; m->dtype[o]=0; } } }
    return 0;
}
/* Risolve H/VOCAB/VIN/VOUT e i parametri di quantizzazione dal modello (architetture
 * arbitrarie): config.json -> hidden_size/vocab_size; header safetensors -> pack/group
 * dell'embed e dimensioni del modulo LoRA target. Fallback ai default se un campo manca. */
static int load_model_shape(const char* dir, int layer){
    char p[1200];
    snprintf(p,sizeof p,"%s/config.json",dir);
    FILE* f=fopen(p,"rb");
    if(f){ fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
        char* js=(char*)malloc((size_t)n+1);
        if(js && fread(js,1,(size_t)n,f)==(size_t)n){ js[n]=0;
            long h=jlong(js,"hidden_size",0); if(h>0) H=(int)h;
            long v=jlong(js,"vocab_size",0);  if(v>0) VOCAB=(int)v; }
        free(js); fclose(f); }
    snprintf(p,sizeof p,"%s/model.safetensors",dir);
    if(st_open(p)==0){
        STMeta m;
        if(st_meta("model.embed_tokens.weight",&m)==0){
            if(m.shape[0]>0) VOCAB=m.shape[0];
            if(m.shape[1]>0 && H>0){ int wpu=H/m.shape[1]; if(wpu>=1&&wpu<=8) g_wpu=wpu; }
        }
        STMeta ms;
        if(st_meta("model.embed_tokens.scales",&ms)==0 && ms.shape[1]>0 && H>0)
            g_group=H/ms.shape[1];
        /* Modulo LoRA target: quello esplicito (g_lora_mod impostato dal chiamante) oppure
         * auto-rilevato tra i moduli per cui il forward del motore ha l'hook, in ordine di
         * preferenza per architettura (dense: v_proj; MLA: kv_b_proj; fallback q_proj/…). */
        if(!g_lora_mod[0]){
            /* Preferisci i moduli con in_dim == H (non ambigui: q/k/v/o/gate/up);
             * kv_b_proj (MLA) e down_proj hanno in_dim ≠ H, ultimi. */
            static const char* cand[] = {"v_proj","q_proj","o_proj","k_proj",
                                         "up_proj","gate_proj","kv_b_proj","down_proj"};
            for(unsigned c=0;c<sizeof cand/sizeof*cand;c++){
                char t[160]; snprintf(t,sizeof t,"model.layers.%d.self_attn.%s.weight",layer,cand[c]);
                STMeta mm;
                if(st_meta(t,&mm)==0){ snprintf(g_lora_mod,sizeof g_lora_mod,"%s",cand[c]); break; }
                snprintf(t,sizeof t,"model.layers.%d.mlp.%s.weight",layer,cand[c]);
                if(st_meta(t,&mm)==0){ snprintf(g_lora_mod,sizeof g_lora_mod,"%s",cand[c]); break; }
            }
            if(!g_lora_mod[0]) snprintf(g_lora_mod,sizeof g_lora_mod,"v_proj");
        }
        /* dim dal modulo target: out=weight.shape[0]; in=scales.shape[1]*group (quantizzato)
         * oppure weight.shape[1] (non quantizzato). Robusto: non dipende dalla quant dell'embed. */
        char tn[160];
        snprintf(tn,sizeof tn,"model.layers.%d.self_attn.%s.weight",layer,g_lora_mod);
        STMeta mt; if(st_meta(tn,&mt)!=0)
            snprintf(tn,sizeof tn,"model.layers.%d.mlp.%s.weight",layer,g_lora_mod);
        if(st_meta(tn,&mt)==0 && mt.shape[0]>0){
            VOUT=mt.shape[0];
            /* in_dim: i moduli attn/mlp standard (eccetto down_proj/kv_b_proj) hanno in==H. */
            if(strcmp(g_lora_mod,"down_proj")!=0 && strcmp(g_lora_mod,"kv_b_proj")!=0){
                VIN=H;
            } else {
                char sn[160];
                snprintf(sn,sizeof sn,"model.layers.%d.self_attn.%s.scales",layer,g_lora_mod);
                STMeta sc; if(st_meta(sn,&sc)!=0)
                    snprintf(sn,sizeof sn,"model.layers.%d.mlp.%s.scales",layer,g_lora_mod);
                if(st_meta(sn,&sc)==0 && sc.shape[1]>0) VIN=sc.shape[1]*g_group;
                else if(mt.shape[1]>0) VIN=mt.shape[1];
            }
        }
        st_close();
    }
    if(H<1 || VOCAB<1 || g_wpu<1 || g_group<1){ fail("load_model_shape"); return -1; }
    return 0;
}

/* dequant TUTTO l'embed [VOCAB,H] affine -> Wlm fp32 (supporta 4/8-bit, group variabile) */
static float* load_full_lmhead(const char* dir){
    char p[1024]; snprintf(p,sizeof p,"%s/model.safetensors",dir);
    if (st_open(p)) return NULL;
    STMeta m; if(st_meta("model.embed_tokens.weight",&m)){ fail("embed"); return NULL; }
    float* Wlm=falloc((size_t)VOCAB*H); if(!Wlm) return NULL;
    /* embed NON quantizzato (F16/BF16/F32): lettura diretta, nessun dequant affine. */
    if(strncmp(m.dtype,"U32",3)!=0 && strncmp(m.dtype,"I32",3)!=0){
        for(size_t i=0;i<(size_t)VOCAB*H;i++) Wlm[i]=st_elem(&m,i);
        return Wlm;
    }
    STMeta ms,mb; if(st_meta("model.embed_tokens.scales",&ms)){ fail("es"); free(Wlm); return NULL; }
    if(st_meta("model.embed_tokens.biases",&mb)){ fail("eb"); free(Wlm); return NULL; }
    int pack_row=m.shape[1], grp_row=ms.shape[1];
    int wpu = pack_row>0 ? H/pack_row : 8, group = grp_row>0 ? H/grp_row : 64;
    int bits = wpu>0 ? 32/wpu : 4;
    uint32_t mask = (bits>=32) ? 0xFFFFFFFFu : ((1u<<bits)-1u);
    if(pack_row<1 || grp_row<1 || wpu<1){ fail("embed shape"); free(Wlm); return NULL; }
    const uint32_t* U=(const uint32_t*)(g_raw+m.off0);
    for(int v=0; v<VOCAB; v++){
        const uint32_t* row=U+(size_t)v*pack_row;
        size_t so=(size_t)v*grp_row;
        float* w=Wlm+(size_t)v*H;
        for(int pack=0; pack<pack_row; pack++){ uint32_t u=row[pack];
            for(int nb=0; nb<wpu; nb++){ int j=pack*wpu+nb, g=j/group;
                w[j]=(float)((u>>(bits*nb))&mask)*st_elem(&ms,so+g)+st_elem(&mb,so+g); } }
    }
    return Wlm;
}
static int load_final_norm(const char* dir, float* lnf){
    STMeta m; if(st_meta("model.norm.weight",&m)){ fail("norm"); return -1; }
    for(int i=0;i<H;i++) lnf[i]=st_elem(&m,(size_t)i);
    return 0;
}
static void rms_vec(const float* x,const float* w,float* out){
    float s=0; for(int i=0;i<H;i++) s+=x[i]*x[i]; s/=H; s=1.0f/sqrtf(s+1e-5f);
    for(int i=0;i<H;i++) out[i]=x[i]*s*w[i];
}
static void logits_full(const float* Wlm,const float* nf,float* logits){
    cblas_sgemv(CblasRowMajor, CblasNoTrans, VOCAB, H, 1.0f, Wlm, H, nf, 1, 0.0f, logits, 1);
}
static double ce_loss(const float* logits,int target){
    float mx=-1e30f; for(int v=0;v<VOCAB;v++) if(logits[v]>mx) mx=logits[v];
    double den=0; for(int v=0;v<VOCAB;v++) den+=expf(logits[v]-mx);
    return -(double)(logits[target]-mx-log(den));
}

/* ---------- dataset ---------- */
typedef struct { int n; int* ids; int target; } Sample;
static Sample* load_dataset(const char* path,int* out_count){
    FILE* f=fopen(path,"rb"); if(!f){ fail("dataset open"); return NULL; }
    int32_t count; if(fread(&count,4,1,f)!=1){ fclose(f); fail("ds count"); return NULL; }
    Sample* s=calloc((size_t)count,sizeof(Sample)); if(!s){ fclose(f); fail("oom ds"); return NULL; }
    for(int i=0;i<count;i++){
        int32_t n; if(fread(&n,4,1,f)!=1){ fclose(f); fail("ds n"); return s; }
        if(n<1||n>MAXT){ fclose(f); fail("ds n range"); return s; }
        s[i].n=(int)n; s[i].ids=(int*)malloc((size_t)n*4);
        if(!s[i].ids || fread(s[i].ids,4,(size_t)n,f)!=(size_t)n){ fclose(f); fail("ds ids"); return s; }
        if(fread(&s[i].target,4,1,f)!=1){ fclose(f); fail("ds target"); return s; }
    }
    fclose(f); *out_count=(int)count;
    return s;
}
static void free_dataset(Sample* s,int count){ if(!s) return; for(int i=0;i<count;i++) free(s[i].ids); free(s); }

/* ---------- engine eval: loss CE full-vocab su una finestra ---------- */
static double eval_sample(LlmEngineLlmModel mod,const Sample* smp,
                          const float* Wlm,const float* lnf,float* hbuf,float* logits){
    LlmEngineLlmSession s=E->create_session(mod);
    int rc=E->capture_hidden(s,smp->ids,smp->n,hbuf,smp->n*H);
    if(rc!=smp->n){ E->destroy_session(s); fail("capture_hidden"); return NAN; }
    E->destroy_session(s);
    const float* h=hbuf+(size_t)(smp->n-1)*H;
    float nf[H]; rms_vec(h,lnf,nf);
    logits_full(Wlm,nf,logits);
    return ce_loss(logits,smp->target);
}

static int apply_adapter(LlmEngineLlmModel mod,int layer,int rank,
                         const float* A,const float* B){
    E->lora_clear(mod);
    if(E->lora_add(mod,layer,g_lora_mod[0]?g_lora_mod:"v_proj",rank,1.0f,A,B,VIN,VOUT)!=0){ fail("lora_add"); return -1; }
    E->lora_set_enabled(mod,1);
    return 0;
}

/* ---- Stream D: adapter sull'head (lm_head), layer sentinella -1; A[H,rank], B[rank,VOCAB] ---- */
#define HEAD_LAYER (-1)
static float *g_hA=NULL, *g_hB=NULL; static int g_hrank=0;
static float g_ht[64];   /* rank: max 64 */

static void head_logits_delta(const float* nf,const float* A,const float* B,int rank,float* logits){
    if(rank>64) rank=64;
    for(int r=0;r<rank;r++){ float t=0; for(int h=0;h<H;h++) t+=nf[h]*A[(size_t)h*rank+r]; g_ht[r]=t; }
    for(int i=0;i<VOCAB;i++){ float d=0; for(int r=0;r<rank;r++) d+=g_ht[r]*B[(size_t)r*VOCAB+i]; logits[i]+=d; }
}
static double eval_sample_head(LlmEngineLlmModel mod,const Sample* smp,
                               const float* Wlm,const float* lnf,float* hbuf,float* logits){
    LlmEngineLlmSession s=E->create_session(mod);
    int rc=E->capture_hidden(s,smp->ids,smp->n,hbuf,smp->n*H);
    if(rc!=smp->n){ E->destroy_session(s); fail("capture_hidden"); return NAN; }
    E->destroy_session(s);
    const float* h=hbuf+(size_t)(smp->n-1)*H;
    float nf[H]; rms_vec(h,lnf,nf);
    logits_full(Wlm,nf,logits);
    if(g_hA&&g_hB) head_logits_delta(nf,g_hA,g_hB,g_hrank,logits);
    return ce_loss(logits,smp->target);
}
static int apply_adapter_head(LlmEngineLlmModel mod,int rank,const float* A,const float* B){
    E->lora_clear(mod);
    if(E->lora_add(mod,HEAD_LAYER,"lm_head",rank,1.0f,A,B,H,VOCAB)!=0){ fail("lora_add head"); return -1; }
    E->lora_set_enabled(mod,1);
    return 0;
}

/* RNG (xorshift) */
static uint64_t rngs;
static void srandx(uint64_t s){ rngs=s; }
static float frand01(){ rngs^=rngs>>12; rngs^=rngs<<25; rngs^=rngs>>27;
    uint64_t r=rngs*0x2545F4914F6CDD1DULL; return (float)((r>>11)*(1.0/9007199254740992.0)); }
static float gauss(){ float u=0; for(int i=0;i<12;i++) u+=frand01(); return (u-6.0f); }

static double avg_loss(LlmEngineLlmModel mod,const Sample* ds,int n,
                       const float* Wlm,const float* lnf,float* hbuf,float* logits){
    double acc=0; for(int i=0;i<n;i++){ double L=eval_sample(mod,&ds[i],Wlm,lnf,hbuf,logits);
        if(!isnan(L)) acc+=L; else acc+=30.0; }
    return n ? acc/n : 0.0;
}

/* ============ Stream D: TRAIN head-only (frozen "all-but-head") ============
 * Addestra un adapter LoRA sull'head (lm_head, layer -1): A[H,rank], B[rank,VOCAB].
 * Il loss usa eval_sample_head (logits full + delta head). Salva <ckpt>_best_{A,B}.raw
 * (A: H*rank, B: rank*VOCAB) + meta con frozen=all-but-head. */
static int finetune_head_exec(char* resp, size_t cap, const char* dir, const char* train_path,
                              const char* hold_path, const char* out_pref, int rank, int steps,
                              int batch, float lr, unsigned seed)
{
    if (rank < 1 || rank > 64) return err_resp(resp, cap, "rank fuori range [1,64]");
    srandx(seed);
    float* Wlm = load_full_lmhead(dir);
    float lnf[H];
    if (!Wlm || load_final_norm(dir, lnf) != 0) { free(Wlm); st_close(); return err_resp(resp, cap, g_err); }
    int nt = 0, nh = 0;
    Sample* tr = load_dataset(train_path, &nt);
    Sample* ho = load_dataset(hold_path, &nh);
    if (!tr || !ho || nt < 1 || nh < 1) { free(Wlm); st_close(); free_dataset(tr,nt); free_dataset(ho,nh); return err_resp(resp, cap, g_err[0] ? g_err : "dataset vuoto"); }
    LlmEngineLlmModel mod = E->load_model(dir);
    if (!mod) { free(Wlm); st_close(); free_dataset(tr,nt); free_dataset(ho,nh); return err_resp(resp, cap, "load_model"); }

    const size_t NPA = (size_t)H * (size_t)rank, NPB = (size_t)rank * (size_t)VOCAB, NPR = NPA + NPB;
    float* A = falloc(NPA); float* B = falloc(NPB);
    float* Ap = falloc(NPA); float* Am = falloc(NPA);
    float* Bp = falloc(NPB); float* Bm = falloc(NPB);
    float* dtheta = falloc(NPR); float* m = falloc(NPR); float* v = falloc(NPR);
    float* bestA = falloc(NPA); float* bestB = falloc(NPB);
    float* hbuf = falloc((size_t)MAXT * H); float* logits = falloc(VOCAB);
    int* bidx = (int*)malloc((size_t)batch * 4);
    if (g_failed || !A||!B||!Ap||!Am||!Bp||!Bm||!dtheta||!m||!v||!bestA||!bestB||!hbuf||!logits||!bidx) {
        free(A);free(B);free(Ap);free(Am);free(Bp);free(Bm);free(dtheta);free(m);free(v);free(bestA);free(bestB);free(hbuf);free(logits);free(bidx);
        E->free_model(mod); free(Wlm); st_close(); free_dataset(tr,nt); free_dataset(ho,nh);
        return err_resp(resp, cap, "oom");
    }
    for (size_t i = 0; i < NPA; i++) A[i] = gauss() * 0.005f;
    memset(B, 0, NPB * 4);
    memset(m, 0, NPR * 4); memset(v, 0, NPR * 4);
    memcpy(bestA, A, NPA * 4); memcpy(bestB, B, NPB * 4);

    /* base loss (prima dell'adapter head) */
    g_hA = NULL; g_hB = NULL; g_hrank = rank;
    double baseL = 0; for (int i = 0; i < nt; i++) { double L = eval_sample_head(mod,&tr[i],Wlm,lnf,hbuf,logits); baseL += isnan(L)?30.0:L; } baseL /= nt;
    double baseH = 0; for (int i = 0; i < nh; i++) { double L = eval_sample_head(mod,&ho[i],Wlm,lnf,hbuf,logits); baseH += isnan(L)?30.0:L; } baseH /= nh;

    int mev = nt < 16 ? nt : 16;
    double best = 1e30; float c = 1e-2f;
    for (int step = 0; step < steps; step++) {
        for (int b = 0; b < batch; b++) bidx[b] = (int)(frand01() * nt);
        for (size_t i = 0; i < NPR; i++) dtheta[i] = (frand01() < 0.5f) ? 1.0f : -1.0f;
        for (size_t i = 0; i < NPA; i++) { Ap[i] = A[i] + c*dtheta[i]; Am[i] = A[i] - c*dtheta[i]; }
        for (size_t j = 0; j < NPB; j++) { Bp[j] = B[j] + c*dtheta[NPA+j]; Bm[j] = B[j] - c*dtheta[NPA+j]; }
        double Lp = 0, Lm = 0;
        g_hA = Ap; g_hB = Bp; for (int b = 0; b < batch; b++) Lp += eval_sample_head(mod,&tr[bidx[b]],Wlm,lnf,hbuf,logits);
        if (g_failed) break;
        g_hA = Am; g_hB = Bm; for (int b = 0; b < batch; b++) Lm += eval_sample_head(mod,&tr[bidx[b]],Wlm,lnf,hbuf,logits);
        if (g_failed) break;
        Lp /= batch; Lm /= batch;
        if (isnan(Lp) || isnan(Lm)) { c *= 0.5f; if (c < 1e-5f) c = 1e-5f; continue; }
        double gscale = (Lp - Lm) / (2.0 * c);
        for (size_t i = 0; i < NPR; i++) {
            float gi = (float)(gscale * dtheta[i]);
            m[i] = 0.9f*m[i] + 0.1f*gi; v[i] = 0.999f*v[i] + 0.001f*gi*gi;
            float mh = m[i] / (1.0f - powf(0.9f, (float)step+1));
            float vh = v[i] / (1.0f - powf(0.999f, (float)step+1));
            float dA = lr * mh / (sqrtf(vh) + 1e-8f);
            if (i < NPA) A[i] -= dA; else B[i - NPA] -= dA;
        }
        g_hA = A; g_hB = B;
        double ev = 0; for (int i = 0; i < mev; i++) { double L = eval_sample_head(mod,&tr[i],Wlm,lnf,hbuf,logits); ev += isnan(L)?30.0:L; } ev /= mev;
        if (ev < best) { best = ev; memcpy(bestA, A, NPA*4); memcpy(bestB, B, NPB*4); }
    }
    /* checkpoint best + meta */
    gmb_mkdirs_for(out_pref);
    {
        char p[1100]; FILE* f;
        snprintf(p, sizeof p, "%s_best_A.raw", out_pref); f = fopen(p,"wb"); if (f) { fwrite(bestA,4,NPA,f); fclose(f); }
        snprintf(p, sizeof p, "%s_best_B.raw", out_pref); f = fopen(p,"wb"); if (f) { fwrite(bestB,4,NPB,f); fclose(f); }
        snprintf(p, sizeof p, "%s_best.meta", out_pref); f = fopen(p,"w"); if (f) { fprintf(f,"layer=%d\nrank=%d\nfrozen=all-but-head\nmod=lm_head\n", HEAD_LAYER, rank); fclose(f); }
    }
    g_hA = bestA; g_hB = bestB;
    double finL = 0; for (int i = 0; i < nt; i++) { double L = eval_sample_head(mod,&tr[i],Wlm,lnf,hbuf,logits); finL += isnan(L)?30.0:L; } finL /= nt;
    double finH = 0; for (int i = 0; i < nh; i++) { double L = eval_sample_head(mod,&ho[i],Wlm,lnf,hbuf,logits); finH += isnan(L)?30.0:L; } finH /= nh;
    g_hA = NULL; g_hB = NULL;

    /* verifica ON/OFF generazione: registra l'adapter head sul modello e confronta. */
    int diff = 0;
    if (apply_adapter_head(mod, rank, bestA, bestB) == 0) {
        int ngen = 6; int prompt[MAXT]; int plen = tr[0].n < 8 ? tr[0].n : 8;
        for (int i = 0; i < plen; i++) prompt[i] = tr[0].ids[i];
        int32_t o1[16], o2[16];
        E->lora_set_enabled(mod, 0); { LlmEngineLlmSession s=E->create_session(mod); E->append_ids(s,prompt,plen); E->generate_ids(s,ngen,-1,o1); E->destroy_session(s); }
        E->lora_set_enabled(mod, 1); { LlmEngineLlmSession s=E->create_session(mod); E->append_ids(s,prompt,plen); E->generate_ids(s,ngen,-1,o2); E->destroy_session(s); }
        for (int i = 0; i < ngen; i++) diff += o1[i] != o2[i];
        E->lora_clear(mod);
    }
    /* report su file <ckpt>.json (come il LoRA): metriche base vs fine, per i gate D2/D3. */
    {
        char rp[1100]; snprintf(rp, sizeof rp, "%s.json", out_pref);
        FILE* rf = fopen(rp, "w");
        if (rf) {
            fprintf(rf, "{\"ok\":true,\"ckpt\":\"%s_best\",\"frozen\":\"all-but-head\","
                        "\"train\":%.6f,\"hold\":%.6f,\"baseTrain\":%.6f,\"baseHold\":%.6f,"
                        "\"demoChanged\":%d}\n",
                    out_pref, finL, finH, baseL, baseH, diff > 0 ? 1 : 0);
            fclose(rf);
        }
    }
    free(A);free(B);free(Ap);free(Am);free(Bp);free(Bm);free(dtheta);free(m);free(v);free(bestA);free(bestB);free(hbuf);free(logits);free(bidx);
    E->free_model(mod); free(Wlm); st_close(); free_dataset(tr,nt); free_dataset(ho,nh);
    if (g_failed) return err_resp(resp, cap, g_err);
    snprintf(resp, cap,
        "{\"type\":\"result\",\"data\":{\"ok\":true,\"ckpt\":\"%s_best\",\"frozen\":\"all-but-head\","
        "\"train\":%.5f,\"hold\":%.5f,\"baseTrain\":%.5f,\"baseHold\":%.5f,\"demoChanged\":%d}}\n",
        out_pref, finL, finH, baseL, baseH, diff > 0 ? 1 : 0);
    return 0;
}

/* ============ TRAIN DPO — preference optimization (PUL) ============
 * Coppie chosen/rejected (due dataset .bin come load_dataset). Loss DPO standard
 * (reference = adapter OFF), gradiente SPSA (come il fine-tune LoRA): self-contained,
 * nessuna ABI esterna. Salva <ckpt>_best_{A,B}.raw + meta. */
static double dpo_pair_loss(LlmEngineLlmModel mod, const Sample* c, const Sample* r,
                            const float* Wlm, const float* lnf, float* hbuf, float* logits,
                            int layer, int rank, const float* A, const float* B, float beta)
{
    apply_adapter(mod, layer, rank, A, B);
    E->lora_set_enabled(mod, 1);
    double cw_on = eval_sample(mod, c, Wlm, lnf, hbuf, logits);
    double cl_on = eval_sample(mod, r, Wlm, lnf, hbuf, logits);
    E->lora_set_enabled(mod, 0);
    double cw_off = eval_sample(mod, c, Wlm, lnf, hbuf, logits);
    double cl_off = eval_sample(mod, r, Wlm, lnf, hbuf, logits);
    E->lora_set_enabled(mod, 1);
    if (isnan(cw_on) || isnan(cl_on) || isnan(cw_off) || isnan(cl_off)) return 30.0;
    double z = beta * ((cw_off - cw_on) - (cl_off - cl_on));   /* logratio_w - logratio_l */
    double nz = -z;                                            /* softplus(-z) stabile */
    return nz > 0.0 ? nz + log1p(exp(-nz)) : log1p(exp(nz));
}

static int gmb_ai_dpo_exec_impl(const char* req, char* resp, size_t cap)
{
    g_failed = 0; g_err[0] = 0; E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_LORA)) return err_resp(resp, cap, "motore con LoRA non disponibile");
    char dir[1024], chp[1024], rjp[1024], out_pref[1024];
    jstr(req, "model_dir", dir, sizeof dir);
    jstr(req, "chosen", chp, sizeof chp);
    jstr(req, "rejected", rjp, sizeof rjp);
    jstr(req, "ckpt", out_pref, sizeof out_pref);
    int rank = (int)jlong(req, "rank", DEFAULT_RANK);
    int steps = (int)jlong(req, "steps", 40);
    int batch = (int)jlong(req, "batch", 2);
    int layer = (int)jlong(req, "layer", DEFAULT_LAYER);
    float lr = (float)jdbl(req, "lr", 5e-3);
    float beta = (float)jdbl(req, "beta", 0.1);
    unsigned seed = (unsigned)jlong(req, "seed", 20260909);
    if (!dir[0] || !chp[0] || !rjp[0] || !out_pref[0])
        return err_resp(resp, cap, "parametri DPO mancanti (model_dir/chosen/rejected/ckpt)");
    if (rank < 1 || rank > 64) return err_resp(resp, cap, "rank fuori range [1,64]");
    if (load_model_shape(dir, layer >= 0 ? layer : DEFAULT_LAYER) != 0) return err_resp(resp, cap, g_err);
    srandx(seed);

    int nc = 0, nr = 0;
    Sample* ch = load_dataset(chp, &nc);
    Sample* rj = load_dataset(rjp, &nr);
    if (!ch || !rj || nc < 1 || nr < 1) {
        st_close(); free_dataset(ch, nc); free_dataset(rj, nr);
        return err_resp(resp, cap, g_err[0] ? g_err : "dataset DPO vuoto");
    }
    LlmEngineLlmModel mod = E->load_model(dir);
    if (!mod) { st_close(); free_dataset(ch, nc); free_dataset(rj, nr); return err_resp(resp, cap, "load_model"); }

    int NPA = VIN * rank, NPB = VOUT * rank, NPR = NPA + NPB;
    int exact = (int)jlong(req, "exact", 1);   /* 1 = passo esatto gmbllm (default); 0 = SPSA self-contained */
    float* A = falloc(NPA); float* B = falloc(NPB);
    float* dA = falloc(NPA); float* dB = falloc(NPB);
    float* m = falloc(NPR); float* v = falloc(NPR);
    float* Ap = falloc(NPA); float* Am = falloc(NPA); float* Bp = falloc(NPB); float* Bm = falloc(NPB);
    float* dtheta = falloc(NPR);
    float* Wlm = load_full_lmhead(dir); float lnf[H];
    float* hbuf = falloc((size_t)MAXT * H); float* logits = falloc(VOCAB);
    int32_t* tw = (int32_t*)malloc((size_t)MAXT * 4); int32_t* tgw = (int32_t*)malloc((size_t)MAXT * 4);
    int32_t* tl = (int32_t*)malloc((size_t)MAXT * 4); int32_t* tgl = (int32_t*)malloc((size_t)MAXT * 4);
    uint8_t* mw = (uint8_t*)malloc((size_t)MAXT); uint8_t* ml = (uint8_t*)malloc((size_t)MAXT);
    const char* proj = g_lora_mod[0] ? g_lora_mod : "v_proj";
    if (g_failed || !A || !B || !dA || !dB || !m || !v || !Ap || !Am || !Bp || !Bm || !dtheta
        || !hbuf || !logits || !tw || !tgw || !tl || !tgl || !mw || !ml) {
        free(A); free(B); free(dA); free(dB); free(m); free(v);
        free(Ap); free(Am); free(Bp); free(Bm); free(dtheta); free(hbuf); free(logits);
        free(tw); free(tgw); free(tl); free(tgl); free(mw); free(ml);
        E->free_model(mod); free(Wlm); st_close(); free_dataset(ch, nc); free_dataset(rj, nr);
        return err_resp(resp, cap, "oom");
    }
    for (int i = 0; i < NPA; i++) A[i] = gauss() * 0.005f;
    memset(B, 0, (size_t)NPB * 4); memset(m, 0, (size_t)NPR * 4); memset(v, 0, (size_t)NPR * 4);

    double base = 0, acc = 0; int nup = 0;
    if (exact) {
        /* PASSO DPO ESATTO (gmbllm `llm_ai_mlx_train_dpo_step`, reference = adapter OFF): i gradienti
         * arrivano da mlx_value_and_grad sul forward differenziabile (loss implicita = softplus(-z)).
         * Sequenze: toks = ids, targets[k] = ids[k+1] (ultimo = target), maschera = solo l'ULTIMA
         * posizione → NLL del token target (come la loss SPSA). */
        int nb0 = nc < 8 ? nc : 8;
        for (int i = 0; i < nb0; i++) {
            int Tc = ch[i].n, Tr = rj[i % nr].n;
            for (int k = 0; k < Tc; k++) { tw[k] = ch[i].ids[k]; tgw[k] = (k < Tc-1) ? ch[i].ids[k+1] : ch[i].target; mw[k] = (uint8_t)(k == Tc-1); }
            for (int k = 0; k < Tr; k++) { tl[k] = rj[i % nr].ids[k]; tgl[k] = (k < Tr-1) ? rj[i % nr].ids[k+1] : rj[i % nr].target; ml[k] = (uint8_t)(k == Tr-1); }
            float L = 0;
            if (llm_ai_mlx_train_dpo_step((void*)mod, tw, tgw, mw, Tc, tl, tgl, ml, Tr, layer, proj, A, VIN, rank, B, rank, VOUT, beta, &L, NULL, NULL) != 0) { fail("dpo_step esatto (baseline)"); break; }
            base += (double)L;
        }
        base /= nb0;
        for (int step = 0; step < steps && !g_failed; step++) {
            int i = (int)(frand01() * (float)nc); if (i < 0) i = 0; if (i >= nc) i = nc-1;
            int j = i % nr;
            int Tc = ch[i].n, Tr = rj[j].n;
            for (int k = 0; k < Tc; k++) { tw[k] = ch[i].ids[k]; tgw[k] = (k < Tc-1) ? ch[i].ids[k+1] : ch[i].target; mw[k] = (uint8_t)(k == Tc-1); }
            for (int k = 0; k < Tr; k++) { tl[k] = rj[j].ids[k]; tgl[k] = (k < Tr-1) ? rj[j].ids[k+1] : rj[j].target; ml[k] = (uint8_t)(k == Tr-1); }
            float L = 0;
            if (llm_ai_mlx_train_dpo_step((void*)mod, tw, tgw, mw, Tc, tl, tgl, ml, Tr, layer, proj, A, VIN, rank, B, rank, VOUT, beta, &L, dA, dB) != 0) { fail("dpo_step esatto"); break; }
            if (isnan(L)) continue;
            for (int k = 0; k < NPA; k++) { float gi = dA[k]; m[k] = 0.9f*m[k] + 0.1f*gi; v[k] = 0.999f*v[k] + 0.001f*gi*gi; float mh = m[k] / (1.0f - powf(0.9f, (float)step + 1)); float vh = v[k] / (1.0f - powf(0.999f, (float)step + 1)); A[k] -= lr * mh / (sqrtf(vh) + 1e-8f); }
            for (int k = 0; k < NPB; k++) { float gi = dB[k]; m[NPA+k] = 0.9f*m[NPA+k] + 0.1f*gi; v[NPA+k] = 0.999f*v[NPA+k] + 0.001f*gi*gi; float mh = m[NPA+k] / (1.0f - powf(0.9f, (float)step + 1)); float vh = v[NPA+k] / (1.0f - powf(0.999f, (float)step + 1)); B[k] -= lr * mh / (sqrtf(vh) + 1e-8f); }
            acc += (double)L; nup++;
        }
    } else {
        /* SPSA self-contained (verificato 5/5): loss DPO standard, reference = adapter OFF. */
        if (!Wlm || load_final_norm(dir, lnf) != 0) {
            free(A); free(B); free(dA); free(dB); free(m); free(v);
            free(Ap); free(Am); free(Bp); free(Bm); free(dtheta); free(hbuf); free(logits);
            free(tw); free(tgw); free(tl); free(tgl); free(mw); free(ml);
            E->free_model(mod); free(Wlm); st_close(); free_dataset(ch, nc); free_dataset(rj, nr);
            return err_resp(resp, cap, g_err);
        }
        int nb0 = nc < 8 ? nc : 8;
        for (int i = 0; i < nb0; i++) base += dpo_pair_loss(mod, &ch[i], &rj[i % nr], Wlm, lnf, hbuf, logits, layer, rank, A, B, beta);
        base /= nb0;
        float c = 1e-2f;
        for (int step = 0; step < steps; step++) {
            int nb = batch < nc ? batch : nc;
            int rng = (int)(frand01() * nc); if (rng < 0) rng = 0;
            for (int i = 0; i < NPR; i++) dtheta[i] = (frand01() < 0.5f) ? 1.0f : -1.0f;
            for (int i = 0; i < NPA; i++) { Ap[i] = A[i] + c*dtheta[i]; Am[i] = A[i] - c*dtheta[i]; }
            for (int j = 0; j < NPB; j++) { Bp[j] = B[j] + c*dtheta[NPA+j]; Bm[j] = B[j] - c*dtheta[NPA+j]; }
            double Lp = 0, Lm = 0;
            for (int b = 0; b < nb; b++) { int i = (rng + b) % nc; int j = i % nr; Lp += dpo_pair_loss(mod, &ch[i], &rj[j], Wlm, lnf, hbuf, logits, layer, rank, Ap, Bp, beta); }
            if (g_failed) break;
            for (int b = 0; b < nb; b++) { int i = (rng + b) % nc; int j = i % nr; Lm += dpo_pair_loss(mod, &ch[i], &rj[j], Wlm, lnf, hbuf, logits, layer, rank, Am, Bm, beta); }
            if (g_failed) break;
            Lp /= nb; Lm /= nb;
            if (isnan(Lp) || isnan(Lm)) { c *= 0.5f; if (c < 1e-5f) c = 1e-5f; continue; }
            double gscale = (Lp - Lm) / (2.0 * c);
            for (int i = 0; i < NPR; i++) {
                float gi = (float)(gscale * dtheta[i]);
                m[i] = 0.9f*m[i] + 0.1f*gi; v[i] = 0.999f*v[i] + 0.001f*gi*gi;
                float mh = m[i] / (1.0f - powf(0.9f, (float)step + 1));
                float vh = v[i] / (1.0f - powf(0.999f, (float)step + 1));
                float d = lr * mh / (sqrtf(vh) + 1e-8f);
                if (i < NPA) A[i] -= d; else B[i - NPA] -= d;
            }
            acc += (Lp + Lm) * 0.5; nup++;
        }
    }
    gmb_mkdirs_for(out_pref);
    { char p[1100]; FILE* f;
      snprintf(p, sizeof p, "%s_best_A.raw", out_pref); f = fopen(p, "wb"); if (f) { fwrite(A, 4, NPA, f); fclose(f); }
      snprintf(p, sizeof p, "%s_best_B.raw", out_pref); f = fopen(p, "wb"); if (f) { fwrite(B, 4, NPB, f); fclose(f); }
      snprintf(p, sizeof p, "%s_best.meta", out_pref); f = fopen(p, "w");
      if (f) { fprintf(f, "layer=%d\nrank=%d\nmod=%s\nbeta=%.4f\ndpo=1\ndpo_engine=%s\n", layer, rank, proj, beta, exact ? "exact" : "spsa"); fclose(f); } }
    free(A); free(B); free(dA); free(dB); free(m); free(v);
    free(Ap); free(Am); free(Bp); free(Bm); free(dtheta); free(hbuf); free(logits);
    free(tw); free(tgw); free(tl); free(tgl); free(mw); free(ml);
    E->free_model(mod); free(Wlm); st_close(); free_dataset(ch, nc); free_dataset(rj, nr);
    if (g_failed) return err_resp(resp, cap, g_err);
    snprintf(resp, cap,
        "{\"type\":\"result\",\"data\":{\"ok\":true,\"ckpt\":\"%s_best\",\"train\":%.5f,\"baseTrain\":%.5f,\"beta\":%.4f}}\n",
        out_pref, nup ? acc / nup : 0.0, base, beta);
    return 0;
}
/* G5 — DISTILLAZIONE teacher-student (KL soft-logit) su modelli REALI. Carica student + teacher,
 * per ogni step: teacher forward -> logits host [T,V]; llm_ai_mlx_train_kl_step -> dA/dB; Adam. */
static int gmb_ai_distill_kl_exec_impl(const char* req, char* resp, size_t cap)
{
    g_failed = 0; g_err[0] = 0; E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_LORA)) return err_resp(resp, cap, "motore con LoRA non disponibile");
    char sdir[1024], tdir[1024], dsp[1024], out_pref[1024];
    jstr(req, "model_dir", sdir, sizeof sdir);
    jstr(req, "teacher", tdir, sizeof tdir);
    jstr(req, "dataset", dsp, sizeof dsp);
    jstr(req, "ckpt", out_pref, sizeof out_pref);
    int rank  = (int)jlong(req, "rank", DEFAULT_RANK);
    int steps = (int)jlong(req, "steps", 40);
    int batch = (int)jlong(req, "batch", 1);
    int layer = (int)jlong(req, "layer", DEFAULT_LAYER);
    float lr  = (float)jdbl(req, "lr", 5e-3);
    float temp = (float)jdbl(req, "temperature", 2.0f);
    unsigned seed = (unsigned)jlong(req, "seed", 20260909);
    (void)batch;
    if (!sdir[0] || !tdir[0] || !dsp[0] || !out_pref[0])
        return err_resp(resp, cap, "parametri distill KL mancanti (model_dir/teacher/dataset/ckpt)");
    if (rank < 1 || rank > 64) return err_resp(resp, cap, "rank fuori range [1,64]");
    if (load_model_shape(sdir, layer >= 0 ? layer : DEFAULT_LAYER) != 0) return err_resp(resp, cap, g_err);
    srandx(seed);

    int nt = 0; Sample* tr = load_dataset(dsp, &nt);
    if (!tr || nt < 1) { st_close(); free_dataset(tr, nt); return err_resp(resp, cap, g_err[0] ? g_err : "dataset vuoto"); }
    LlmEngineLlmModel mod = E->load_model(sdir);
    LlmEngineLlmModel tmod = E->load_model(tdir);
    if (!mod || !tmod) { if (mod) E->free_model(mod); if (tmod) E->free_model(tmod); st_close(); free_dataset(tr, nt); return err_resp(resp, cap, "load_model"); }

    int NPA = VIN * rank, NPB = VOUT * rank, np = NPA + NPB;
    float* A = falloc(NPA); float* B = falloc(NPB); float* dA = falloc(NPA); float* dB = falloc(NPB);
    float* m = falloc(np); float* v = falloc(np); float* bestA = falloc(NPA); float* bestB = falloc(NPB);
    int32_t* toks = (int32_t*)malloc((size_t)MAXT * 4);
    float* tlog = (float*)malloc((size_t)MAXT * (size_t)VOCAB * 4);
    const char* proj = g_lora_mod[0] ? g_lora_mod : "v_proj";
    if (g_failed || !A || !B || !dA || !dB || !m || !v || !bestA || !bestB || !toks || !tlog) {
        free(A); free(B); free(dA); free(dB); free(m); free(v); free(bestA); free(bestB); free(toks); free(tlog);
        E->free_model(mod); E->free_model(tmod); st_close(); free_dataset(tr, nt); return err_resp(resp, cap, "oom");
    }
    for (int i = 0; i < NPA; i++) A[i] = gauss() * 0.005f;
    memset(B, 0, (size_t)NPB * 4); memset(m, 0, (size_t)np * 4); memset(v, 0, (size_t)np * 4);
    memcpy(bestA, A, (size_t)NPA * 4); memcpy(bestB, B, (size_t)NPB * 4);

    double best = 1e30, L = 0; int ok_flag = 1;
    for (int step = 0; step < steps; step++) {
        int si = (int)(frand01() * nt); Sample* s = &tr[si];
        int T = s->n; if (T > MAXT) T = MAXT; if (T < 1) continue;
        for (int i = 0; i < T; i++) toks[i] = s->ids[i];
        /* teacher forward -> logits host [T,V] (detach) — helper gmbllm (mlx isolato lì) */
        if (llm_train_teacher_logits((void*)tmod, toks, T, tlog, VOCAB) != 0) { ok_flag = 0; break; }
        if (llm_ai_mlx_train_kl_step((void*)mod, toks, T, tlog, VOCAB, temp, NULL, layer, proj,
                                     A, VIN, rank, B, rank, VOUT, &L, dA, dB) != 0) { ok_flag = 0; break; }
        for (int k = 0; k < NPA; k++) { float gi = dA[k]; m[k] = 0.9f*m[k] + 0.1f*gi; v[k] = 0.999f*v[k] + 0.001f*gi*gi;
            float mh = m[k] / (1.0f - powf(0.9f, (float)step + 1)); float vh = v[k] / (1.0f - powf(0.999f, (float)step + 1)); A[k] -= lr * mh / (sqrtf(vh) + 1e-8f); }
        for (int k = 0; k < NPB; k++) { float gi = dB[k]; int j = NPA + k; m[j] = 0.9f*m[j] + 0.1f*gi; v[j] = 0.999f*v[j] + 0.001f*gi*gi;
            float mh = m[j] / (1.0f - powf(0.9f, (float)step + 1)); float vh = v[j] / (1.0f - powf(0.999f, (float)step + 1)); B[k] -= lr * mh / (sqrtf(vh) + 1e-8f); }
        if (isnan(L)) { ok_flag = 0; break; }
        if (L < best) { best = L; memcpy(bestA, A, (size_t)NPA * 4); memcpy(bestB, B, (size_t)NPB * 4); }
    }
    gmb_mkdirs_for(out_pref);
    { char p[1100]; FILE* f;
      snprintf(p, sizeof p, "%s_best_A.raw", out_pref); f = fopen(p, "wb"); if (f) { fwrite(bestA, 4, (size_t)NPA, f); fclose(f); }
      snprintf(p, sizeof p, "%s_best_B.raw", out_pref); f = fopen(p, "wb"); if (f) { fwrite(bestB, 4, (size_t)NPB, f); fclose(f); }
      snprintf(p, sizeof p, "%s_best.meta", out_pref); f = fopen(p, "w"); if (f) { fprintf(f, "layer=%d\nrank=%d\nmod=%s\n", layer, rank, proj); fclose(f); } }
    free(A); free(B); free(dA); free(dB); free(m); free(v); free(bestA); free(bestB); free(toks); free(tlog);
    E->free_model(mod); E->free_model(tmod); st_close(); free_dataset(tr, nt);
    if (g_failed) return err_resp(resp, cap, g_err);
    snprintf(resp, cap, "{\"type\":\"result\",\"data\":{\"ok\":%s,\"ckpt\":\"%s_best\",\"kl\":%.5f,\"steps\":%d,\"temperature\":%.2f,\"teacher\":\"%s\"}}\n",
             ok_flag ? "true" : "false", out_pref, best, steps, temp, tdir);
    return 0;
}

int gmb_ai_dpo_exec(const char* req, char* resp, size_t cap) { return gmb_run_big_stack(gmb_ai_dpo_exec_impl, req, resp, cap); }

/* ============ TRAIN GRPO — RLHF senza critic (RLVR) (PUL) ============
 * Per ogni prompt (Sample del dataset): campiona G completamenti (maxnew token, sampling con
 * temperatura), reward RULE-based (match del 1° token generato col `target` del Sample), vantaggi
 * GROUP-RELATIVE ((r-mean)/std), old_logp = somma dei logprob emessi (generate_ids_probs). Passo
 * GRPO esatto (llm_ai_mlx_train_grpo_step: clipped surrogate + k3-KL vs ref) + Adam. Salva
 * <ckpt>_best_{A,B}.raw + meta. Nessun reward model esterno (RLVR): verificabile e self-contained. */
static int gmb_ai_grpo_exec_impl(const char* req, char* resp, size_t cap)
{
    g_failed = 0; g_err[0] = 0; E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_LORA)) return err_resp(resp, cap, "motore con LoRA non disponibile");
    if (E->create_session == NULL || E->generate_ids_probs == NULL)
        return err_resp(resp, cap, "motore senza create_session/generate_ids_probs (serve per il campionamento)");
    char dir[1024], dsp[1024], out_pref[1024];
    jstr(req, "model_dir", dir, sizeof dir);
    jstr(req, "dataset", dsp, sizeof dsp);
    jstr(req, "ckpt", out_pref, sizeof out_pref);
    int rank  = (int)jlong(req, "rank", DEFAULT_RANK);
    int steps = (int)jlong(req, "steps", 40);
    int layer = (int)jlong(req, "layer", DEFAULT_LAYER);
    int G     = (int)jlong(req, "group", 4);
    int maxnew= (int)jlong(req, "maxnew", 1);
    float lr  = (float)jdbl(req, "lr", 5e-3);
    float clip= (float)jdbl(req, "clip", 0.2);
    float kl  = (float)jdbl(req, "kl", 0.0);
    float temp= (float)jdbl(req, "temperature", 1.0);
    unsigned seed = (unsigned)jlong(req, "seed", 20260909);
    if (!dir[0] || !dsp[0] || !out_pref[0])
        return err_resp(resp, cap, "parametri GRPO mancanti (model_dir/dataset/ckpt)");
    if (rank < 1 || rank > 64) return err_resp(resp, cap, "rank fuori range [1,64]");
    if (G < 2 || G > 32) return err_resp(resp, cap, "group fuori range [2,32]");
    if (maxnew < 1 || maxnew > 8) return err_resp(resp, cap, "maxnew fuori range [1,8]");
    if (load_model_shape(dir, layer >= 0 ? layer : DEFAULT_LAYER) != 0) return err_resp(resp, cap, g_err);
    srandx(seed);

    int nt = 0; Sample* tr = load_dataset(dsp, &nt);
    if (!tr || nt < 1) { st_close(); free_dataset(tr, nt); return err_resp(resp, cap, g_err[0] ? g_err : "dataset GRPO vuoto"); }
    LlmEngineLlmModel mod = E->load_model(dir);
    if (!mod) { st_close(); free_dataset(tr, nt); return err_resp(resp, cap, "load_model"); }

    int NPA = VIN * rank, NPB = VOUT * rank, np = NPA + NPB;
    float* A = falloc(NPA); float* B = falloc(NPB); float* dA = falloc(NPA); float* dB = falloc(NPB);
    float* m = falloc(np); float* v = falloc(np); float* bestA = falloc(NPA); float* bestB = falloc(NPB);
    size_t capT = (size_t)G * (size_t)MAXT;
    int32_t* toks = (int32_t*)malloc(capT * 4);
    int32_t* tgt  = (int32_t*)malloc(capT * 4);
    uint8_t* mask = (uint8_t*)malloc(capT);
    float* oldlp  = (float*)malloc((size_t)G * 4);
    float* adv    = (float*)malloc((size_t)G * 4);
    int32_t* gen  = (int32_t*)malloc((size_t)G * (size_t)maxnew * 4);
    float* genlp  = (float*)malloc((size_t)G * (size_t)maxnew * 4);
    const char* proj = g_lora_mod[0] ? g_lora_mod : "v_proj";
    if (g_failed || !A || !B || !dA || !dB || !m || !v || !bestA || !bestB
        || !toks || !tgt || !mask || !oldlp || !adv || !gen || !genlp) {
        free(A); free(B); free(dA); free(dB); free(m); free(v); free(bestA); free(bestB);
        free(toks); free(tgt); free(mask); free(oldlp); free(adv); free(gen); free(genlp);
        E->free_model(mod); st_close(); free_dataset(tr, nt); return err_resp(resp, cap, "oom");
    }
    for (int i = 0; i < NPA; i++) A[i] = gauss() * 0.005f;
    memset(B, 0, (size_t)NPB * 4); memset(m, 0, (size_t)np * 4); memset(v, 0, (size_t)np * 4);
    memcpy(bestA, A, (size_t)NPA * 4); memcpy(bestB, B, (size_t)NPB * 4);

    double base = 0, acc = 0; int nup = 0, nskip = 0;
    for (int step = 0; step < steps && !g_failed; step++) {
        int si = (int)(frand01() * (float)nt); if (si < 0) si = 0; if (si >= nt) si = nt - 1;
        Sample* smp = &tr[si];
        int plen = smp->n; if (plen < 1) { nskip++; continue; }
        int T = plen + maxnew; if (T > MAXT) T = MAXT;
        if (T <= maxnew) { nskip++; continue; }
        /* Campiona G completamenti (KV clear + append prompt + maxnew token con sampling). */
        LlmEngineLlmSession sess = E->create_session(mod);
        if (sess == NULL) { fail("grpo: create_session"); break; }
        if (E->set_sampling) {
            LlmEngineLlmSampling sp; memset(&sp, 0, sizeof sp);
            sp.temperature = temp; sp.top_p = 1.0f; sp.top_k = 0; sp.repetition_penalty = 1.0f;
            sp.seed = (int64_t)(seed + step);
            E->set_sampling(sess, &sp);
        }
        int gok = 1;
        for (int g = 0; g < G; g++) {
            if (E->kv_clear) E->kv_clear(sess);
            if (E->append_ids(sess, smp->ids, plen) != 0) { gok = 0; break; }
            int gn = E->generate_ids_probs(sess, maxnew, -1, gen + (size_t)g * maxnew, genlp + (size_t)g * maxnew);
            if (gn < 1) { gok = 0; break; }
            /* reward RULE: primo token generato == target del Sample (RLVR esatto). */
            float r = (gen[(size_t)g * maxnew] == smp->target) ? 1.0f : 0.0f;
            oldlp[g] = 0.0f; for (int k = 0; k < maxnew && k < gn; k++) oldlp[g] += genlp[(size_t)g * maxnew + k];
            adv[g] = r;
            /* costruisci toks/tgt/mask della completazione g (T fisso). */
            int32_t* tg = toks + (size_t)g * T; int32_t* tt = tgt + (size_t)g * T; uint8_t* mk = mask + (size_t)g * T;
            for (int k = 0; k < T; k++) {
                tg[k] = (k < plen) ? smp->ids[k] : gen[(size_t)g * maxnew + (k - plen)];
                tt[k] = (k + 1 < T) ? ((k + 1 < plen) ? smp->ids[k + 1] : gen[(size_t)g * maxnew + (k + 1 - plen)]) : tg[k];
                mk[k] = (uint8_t)((k >= plen - 1) && (k < T - 1));   /* predice i token di completazione */
            }
        }
        E->destroy_session(sess);
        if (!gok) { nskip++; continue; }
        /* vantaggi group-relative. */
        double mr = 0; for (int g = 0; g < G; g++) mr += adv[g]; mr /= G;
        double vr = 0; for (int g = 0; g < G; g++) vr += (adv[g] - mr) * (adv[g] - mr); vr /= G;
        double sr = sqrt(vr);
        if (sr < 1e-6) { nskip++; continue; }   /* gruppo senza varianza -> nessun segnale */
        for (int g = 0; g < G; g++) adv[g] = (float)((adv[g] - mr) / (sr + 1e-6));
        float L = 0;
        if (llm_ai_mlx_train_grpo_step((void*)mod, toks, tgt, mask, T, G, oldlp, adv, clip, kl,
                                       layer, proj, A, VIN, rank, B, rank, VOUT, &L, dA, dB) != 0) { fail("grpo_step"); break; }
        if (isnan(L)) { nskip++; continue; }
        for (int k = 0; k < NPA; k++) { float gi = dA[k]; m[k] = 0.9f*m[k] + 0.1f*gi; v[k] = 0.999f*v[k] + 0.001f*gi*gi;
            float mh = m[k] / (1.0f - powf(0.9f, (float)nup + 1)); float vh = v[k] / (1.0f - powf(0.999f, (float)nup + 1)); A[k] -= lr * mh / (sqrtf(vh) + 1e-8f); }
        for (int k = 0; k < NPB; k++) { float gi = dB[k]; int j = NPA + k; m[j] = 0.9f*m[j] + 0.1f*gi; v[j] = 0.999f*v[j] + 0.001f*gi*gi;
            float mh = m[j] / (1.0f - powf(0.9f, (float)nup + 1)); float vh = v[j] / (1.0f - powf(0.999f, (float)nup + 1)); B[k] -= lr * mh / (sqrtf(vh) + 1e-8f); }
        acc += L; nup++;
        if (nup == 1) base = L;
    }
    gmb_mkdirs_for(out_pref);
    { char p[1100]; FILE* f;
      snprintf(p, sizeof p, "%s_best_A.raw", out_pref); f = fopen(p, "wb"); if (f) { fwrite(A, 4, (size_t)NPA, f); fclose(f); }
      snprintf(p, sizeof p, "%s_best_B.raw", out_pref); f = fopen(p, "wb"); if (f) { fwrite(B, 4, (size_t)NPB, f); fclose(f); }
      snprintf(p, sizeof p, "%s_best.meta", out_pref); f = fopen(p, "w");
      if (f) { fprintf(f, "layer=%d\nrank=%d\nmod=%s\ngrpo=1\ngroup=%d\nclip=%.3f\nkl=%.4f\n", layer, rank, proj, G, clip, kl); fclose(f); } }
    free(A); free(B); free(dA); free(dB); free(m); free(v); free(bestA); free(bestB);
    free(toks); free(tgt); free(mask); free(oldlp); free(adv); free(gen); free(genlp);
    E->free_model(mod); st_close(); free_dataset(tr, nt);
    if (g_failed) return err_resp(resp, cap, g_err);
    snprintf(resp, cap,
        "{\"type\":\"result\",\"data\":{\"ok\":true,\"ckpt\":\"%s_best\",\"grpo\":%.5f,\"base\":%.5f,\"steps\":%d,\"updated\":%d,\"skipped\":%d,\"group\":%d}}\n",
        out_pref, nup ? acc / nup : 0.0, base, steps, nup, nskip, G);
    return 0;
}
int gmb_ai_grpo_exec(const char* req, char* resp, size_t cap) { return gmb_run_big_stack(gmb_ai_grpo_exec_impl, req, resp, cap); }

static int gmb_ai_finetune_exec_impl(const char* req, char* resp, size_t cap)
{
    g_failed = 0; g_err[0] = 0;    E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_LORA)) return err_resp(resp, cap, "motore con LoRA non disponibile");
    char dir[1024], train_path[1024], hold_path[1024], out_pref[1024];
    jstr(req, "model_dir", dir, sizeof dir);
    jstr(req, "dataset", train_path, sizeof train_path);
    jstr(req, "holdout", hold_path, sizeof hold_path);
    jstr(req, "ckpt", out_pref, sizeof out_pref);
    int rank   = (int)jlong(req, "rank", DEFAULT_RANK);
    int steps  = (int)jlong(req, "steps", 80);
    int batch  = (int)jlong(req, "batch", 2);
    int layer  = (int)jlong(req, "layer", DEFAULT_LAYER);
    float lr   = (float)jdbl(req, "lr", 1e-2);
    unsigned seed = (unsigned)jlong(req, "seed", 20260909);
    char frozen[32]; jstr(req, "frozen", frozen, sizeof frozen);
    if (jlong(req, "full", 0) != 0) {   /* G6: full-parameter (Adam su TUTTI i pesi) */
        if (!dir[0] || !train_path[0] || !out_pref[0])
            return err_resp(resp, cap, "parametri full mancanti (model_dir/dataset/ckpt)");
        int nt2 = 0; Sample* tr2 = load_dataset(train_path, &nt2);
        if (!tr2 || nt2 < 1) { st_close(); free_dataset(tr2, nt2); return err_resp(resp, cap, "dataset vuoto"); }
        LlmEngineLlmModel fmod = E->load_model(dir);
        if (!fmod) { st_close(); free_dataset(tr2, nt2); return err_resp(resp, cap, "load_model"); }
        int T2 = tr2[0].n; if (T2 > MAXT) T2 = MAXT;
        int frc = llm_ai_mlx_train_full_fit((void*)fmod, tr2[0].ids, tr2[0].ids, NULL, T2, steps, lr, out_pref);
        E->free_model(fmod); st_close(); free_dataset(tr2, nt2);
        if (frc != 0) return err_resp(resp, cap, "full-param fit fallito");
        snprintf(resp, cap, "{\"type\":\"result\",\"data\":{\"ok\":true,\"full\":true,\"ckpt\":\"%s\",\"steps\":%d,\"lr\":%.4f}}\n", out_pref, steps, lr);
        return 0;
    }
    if (!dir[0] || !train_path[0] || !hold_path[0] || !out_pref[0]) {
        return err_resp(resp, cap, "parametri finetune mancanti (model_dir/dataset/holdout/ckpt)");
    }
    if (load_model_shape(dir, layer >= 0 ? layer : DEFAULT_LAYER) != 0)
        return err_resp(resp, cap, g_err);
    if (strcmp(frozen, "all-but-head") == 0)
        return finetune_head_exec(resp, cap, dir, train_path, hold_path, out_pref, rank, steps, batch, lr, seed);
    srandx(seed);

    float* Wlm = load_full_lmhead(dir);
    float lnf[H];
    if (!Wlm || load_final_norm(dir, lnf) != 0) { free(Wlm); st_close(); return err_resp(resp, cap, g_err); }

    int nt=0, nh=0;
    Sample* tr = load_dataset(train_path, &nt);
    Sample* ho = load_dataset(hold_path, &nh);
    if (!tr || !ho) { free(Wlm); st_close(); free_dataset(tr,nt); free_dataset(ho,nh); return err_resp(resp, cap, g_err); }

    LlmEngineLlmModel mod = E->load_model(dir);
    if (!mod) { free(Wlm); st_close(); free_dataset(tr,nt); free_dataset(ho,nh); return err_resp(resp, cap, "load_model"); }

    int NP=VIN*rank, NR=rank*VOUT, NPR=NP+NR;
    float* A=falloc((size_t)VIN*rank); float* B=falloc((size_t)rank*VOUT);
    float* hbuf=falloc((size_t)MAXT*H); float* logits=falloc(VOCAB);
    float* m=falloc(NPR); float* v=falloc(NPR); float* dtheta=falloc(NPR);
    float* Ap=falloc((size_t)VIN*rank); float* Bp=falloc((size_t)rank*VOUT);
    float* Am=falloc((size_t)VIN*rank); float* Bm=falloc((size_t)rank*VOUT);
    float* bestA=falloc((size_t)VIN*rank); float* bestB=falloc((size_t)rank*VOUT);
    int* ev_idx = (int*)malloc((size_t)(nt<48?nt:48)*4);
    int* bidx = (int*)malloc((size_t)batch*4);
    if (g_failed || !A||!B||!hbuf||!logits||!m||!v||!dtheta||!Ap||!Bp||!Am||!Bm||!bestA||!bestB||!ev_idx||!bidx) {
        free(A);free(B);free(hbuf);free(logits);free(m);free(v);free(dtheta);free(Ap);free(Bp);free(Am);free(Bm);free(bestA);free(bestB);
        free(ev_idx);free(bidx); E->free_model(mod); free(Wlm); st_close(); free_dataset(tr,nt); free_dataset(ho,nh);
        return err_resp(resp, cap, "oom");
    }

    for(int i=0;i<VIN*rank;i++) A[i]=gauss()*0.005f;
    memset(B,0,(size_t)rank*VOUT*4);
    apply_adapter(mod,layer,rank,A,B);
    if (g_failed) goto done;

    double baseL=avg_loss(mod,tr,nt,Wlm,lnf,hbuf,logits);
    double baseH=avg_loss(mod,ho,nh,Wlm,lnf,hbuf,logits);
    memset(m,0,NPR*4); memset(v,0,NPR*4);
    double best=1e30;
    memcpy(bestA,A,(size_t)VIN*rank*4); memcpy(bestB,B,(size_t)rank*VOUT*4);
    float c=1e-2f;
    int mev = nt<48?nt:48;
    for(int i=0;i<mev;i++) ev_idx[i]=i;
    int ok_flag = 1;
    for(int step=0; step<steps; step++){
        for(int b=0;b<batch;b++) bidx[b]=(int)(frand01()*nt);
        for(int i=0;i<NPR;i++) dtheta[i]=(frand01()<0.5f)?1.0f:-1.0f;
        for(int i=0;i<VIN*rank;i++){ Ap[i]=A[i]+c*dtheta[i]; Am[i]=A[i]-c*dtheta[i]; }
        for(int j=0;j<rank*VOUT;j++){ Bp[j]=B[j]+c*dtheta[NP+j]; Bm[j]=B[j]-c*dtheta[NP+j]; }
        if (apply_adapter(mod,layer,rank,Ap,Bp)!=0) break;
        double Lp=0; for(int b=0;b<batch;b++) Lp+=eval_sample(mod,&tr[bidx[b]],Wlm,lnf,hbuf,logits);
        if (apply_adapter(mod,layer,rank,Am,Bm)!=0) break;
        double Lm=0; for(int b=0;b<batch;b++) Lm+=eval_sample(mod,&tr[bidx[b]],Wlm,lnf,hbuf,logits);
        if (g_failed) break;
        Lp/=batch; Lm/=batch;
        if(isnan(Lp)||isnan(Lm)){ c*=0.5f; if(c<1e-5) c=1e-5f; continue; }
        double gscale=(Lp-Lm)/(2.0*c);
        for(int i=0;i<NPR;i++){
            float gi=(float)(gscale*dtheta[i]);
            m[i]=0.9f*m[i]+0.1f*gi; v[i]=0.999f*v[i]+0.001f*gi*gi;
            float mh=m[i]/(1.0f-powf(0.9f,(float)step+1));
            float vh=v[i]/(1.0f-powf(0.999f,(float)step+1));
            float dA=lr*mh/(sqrtf(vh)+1e-8f);
            if(i<NP) A[i]-=dA; else B[i-NP]-=dA;
        }
        apply_adapter(mod,layer,rank,A,B);
        double ev=avg_loss(mod,tr,mev,Wlm,lnf,hbuf,logits);
        if(ev<best){ best=ev; memcpy(bestA,A,(size_t)VIN*rank*4); memcpy(bestB,B,(size_t)rank*VOUT*4); }
        if(step>5 && isnan(ev)){ ok_flag = 0; break; }
    }
    /* checkpoint best + metadati (per apply) */
    gmb_mkdirs_for(out_pref);
    {
        char p[1100]; FILE* f;
        snprintf(p,sizeof p,"%s_best_A.raw",out_pref); f=fopen(p,"wb"); if(f){ fwrite(bestA,4,(size_t)VIN*rank,f); fclose(f); }
        snprintf(p,sizeof p,"%s_best_B.raw",out_pref); f=fopen(p,"wb"); if(f){ fwrite(bestB,4,(size_t)rank*VOUT,f); fclose(f); }
        snprintf(p,sizeof p,"%s_best.meta",out_pref); f=fopen(p,"w"); if(f){ fprintf(f,"layer=%d\nrank=%d\nmod=%s\n",layer,rank,g_lora_mod); fclose(f); }
    }
    apply_adapter(mod,layer,rank,bestA,bestB);
    double finL=avg_loss(mod,tr,nt,Wlm,lnf,hbuf,logits);
    double finH=avg_loss(mod,ho,nh,Wlm,lnf,hbuf,logits);

    int greedy0=-1, greedy1=-1, diff6=-1;
    {
        int ngen=6; int prompt[12]; int plen = tr[0].n<8?tr[0].n:8;
        for(int i=0;i<plen;i++) prompt[i]=tr[0].ids[i];
        int32_t out1[16], out2[16];
        E->lora_set_enabled(mod,0);
        { LlmEngineLlmSession s=E->create_session(mod);
          E->append_ids(s,prompt,plen);
          E->generate_ids(s,ngen,-1,out1);
          E->destroy_session(s); }
        E->lora_set_enabled(mod,1);
        { LlmEngineLlmSession s=E->create_session(mod);
          E->append_ids(s,prompt,plen);
          E->generate_ids(s,ngen,-1,out2);
          E->destroy_session(s); }
        int diff=0;
        for(int i=0;i<ngen;i++) diff += out1[i]!=out2[i];
        greedy0=(int)out1[0]; greedy1=(int)out2[0]; diff6=diff;
    }
    /* report JSON su file <ckpt>.json (compatibilità) + response JSON */
    {
        char rp[1100]; snprintf(rp,sizeof rp,"%s.json",out_pref);
        FILE* f=fopen(rp,"w");
        if(f){
            fprintf(f,"{\"ok\":%s,\"ckpt\":\"%s_best\",\"train\":%.5f,\"hold\":%.5f,"
                    "\"baseTrain\":%.5f,\"baseHold\":%.5f,\"greedyBefore\":%d,"
                    "\"greedyAfter\":%d,\"demoChanged\":%d}\n",
                    ok_flag?"true":"false", out_pref, finL, finH, baseL, baseH, greedy0, greedy1, diff6>0?1:0);
            fclose(f);
        }
    }
    E->lora_clear(mod);

done:
    free(A);free(B);free(hbuf);free(logits);free(m);free(v);free(dtheta);free(Ap);free(Bp);free(Am);free(Bm);free(bestA);free(bestB);
    free(ev_idx);free(bidx);
    E->free_model(mod);
    free(Wlm); st_close();
    free_dataset(tr,nt); free_dataset(ho,nh);
    if (g_failed) { return err_resp(resp, cap, g_err); }
    snprintf(resp, cap,
        "{\"type\":\"result\",\"data\":{\"ok\":%s,\"ckpt\":\"%s_best\",\"train\":%.5f,"
        "\"hold\":%.5f,\"baseTrain\":%.5f,\"baseHold\":%.5f,\"greedyBefore\":%d,"
        "\"greedyAfter\":%d,\"demoChanged\":%d}}\n",
        ok_flag?"true":"false", out_pref, finL, finH, baseL, baseH, greedy0, greedy1, diff6>0?1:0);
    return 0;
}

/* ============================ LORA APPLY / DEPLOY ============================ */

static float* rd_raw(const char* path, int count){
    FILE* f=fopen(path,"rb"); if(!f){ fail("ckpt open"); return NULL; }
    float* d=(float*)malloc((size_t)count*4);
    if(!d || fread(d,4,(size_t)count,f)!=(size_t)count){ if(d) free(d); fclose(f); fail("ckpt read"); return NULL; }
    fclose(f); return d;
}
typedef struct { int n; int ids[MAXT]; } Win;
static int load_first_sample(const char* path, Win* w){
    FILE* f=fopen(path,"rb"); if(!f){ fail("ds open"); return -1; }
    int32_t cnt; if(fread(&cnt,4,1,f)!=1){ fclose(f); fail("ds count"); return -1; }
    int32_t n; if(fread(&n,4,1,f)!=1){ fclose(f); fail("ds n"); return -1; }
    if(n<1||n>MAXT){ fclose(f); fail("ds n range"); return -1; }
    w->n=(int)n;
    if(fread(w->ids,4,(size_t)n,f)!=(size_t)n){ fclose(f); fail("ds ids"); return -1; }
    fclose(f);
    return 0;
}
static void gen(LlmEngineLlmModel mod,const int32_t* ids,int n,int ngen,int32_t* out){
    LlmEngineLlmSession s=E->create_session(mod);
    E->append_ids(s,ids,n);
    E->generate_ids(s,ngen,-1,out);
    E->destroy_session(s);
}

/* HOT-SWAP (multi-adapter): carica il modello UNA volta e scambia N adapter (clear+add+gen),
 * senza ricaricare la base. csv = prefissi <ckpt> separati da virgola. */
static int lora_swap_run(const char* dir, const char* csv, const char* hold, int ngen, char* resp, size_t cap) {
    Win w; if (load_first_sample(hold, &w) != 0) return err_resp(resp, cap, g_err);
    LlmEngineLlmModel mod = E->load_model(dir);
    if (!mod) return err_resp(resp, cap, "load_model");
    int32_t o1[16], o2[16];
    E->lora_clear(mod); E->lora_set_enabled(mod, 0);
    gen(mod, w.ids, w.n, ngen, o1);
    char items[4096]; size_t io = 0; items[0] = 0; int nok = 0;
    char copy[1024]; snprintf(copy, sizeof copy, "%s", csv);
    char* save = NULL; char* tok = strtok_r(copy, ",", &save);
    while (tok) {
        while (*tok == ' ') tok++;
        char pre[1024]; snprintf(pre, sizeof pre, "%s", tok);
        int layer = DEFAULT_LAYER, ahead = 0; char modn[32]; modn[0] = 0;
        char metap[1100]; snprintf(metap, sizeof metap, "%s_best.meta", pre);
        FILE* fm = fopen(metap, "r");
        if (fm) { char lb[64]; while (fgets(lb, sizeof lb, fm)) {
            if (strncmp(lb, "layer=", 6) == 0) { int l = atoi(lb + 6); layer = l; if (l < 0) ahead = 1; }
            else if (strncmp(lb, "frozen=", 7) == 0 && strstr(lb, "all-but-head")) ahead = 1;
            else if (strncmp(lb, "mod=", 4) == 0) { int o = 0; const char* q = lb + 4; while (*q && *q != '\n' && *q != '\r' && o < 31) modn[o++] = *q++; modn[o] = 0; }
        } fclose(fm); }
        int in = ahead ? H : VIN, out = ahead ? VOCAB : VOUT;
        char bp[1100]; snprintf(bp, sizeof bp, "%s_best_B.raw", pre);
        FILE* fb = fopen(bp, "rb"); if (!fb) { E->free_model(mod); return err_resp(resp, cap, "open B"); }
        fseek(fb, 0, SEEK_END); long sz = ftell(fb); fclose(fb);
        if (sz <= 0 || sz % (out * 4) != 0) { E->free_model(mod); return err_resp(resp, cap, "B size"); }
        int rank = (int)(sz / (out * 4));
        char ap[1100]; snprintf(ap, sizeof ap, "%s_best_A.raw", pre);
        float* A = rd_raw(ap, in * rank); float* B = rd_raw(bp, rank * out);
        if (!A || !B) { free(A); free(B); E->free_model(mod); return err_resp(resp, cap, g_err); }
        E->lora_clear(mod);
        if (E->lora_add(mod, ahead ? HEAD_LAYER : layer, ahead ? "lm_head" : (modn[0] ? modn : "v_proj"), rank, 1.0f, A, B, in, out) != 0) { free(A); free(B); E->free_model(mod); return err_resp(resp, cap, "lora_add"); }
        E->lora_set_enabled(mod, 1);
        gen(mod, w.ids, w.n, ngen, o2);
        int diff = 0; for (int i = 0; i < ngen; i++) diff += o1[i] != o2[i];
        io += (size_t)snprintf(items + io, sizeof items - io, "%s{\"ckpt\":\"%s\",\"rank\":%d,\"demoChanged\":%d}", nok ? "," : "", pre, rank, diff > 0 ? 1 : 0);
        nok++; free(A); free(B);
        tok = strtok_r(NULL, ",", &save);
    }
    E->lora_clear(mod); E->free_model(mod);
    snprintf(resp, cap, "{\"type\":\"result\",\"data\":{\"ok\":true,\"message\":\"hot-swap: %d adapter su un solo load\",\"swaps\":[%s]}}\n", nok, items);
    return 0;
}

static int gmb_ai_lora_apply_exec_impl(const char* req, char* resp, size_t cap)
{
    g_failed = 0; g_err[0] = 0;
    E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_LORA)) return err_resp(resp, cap, "motore con LoRA non disponibile");
    char dir[1024], pre[1024], hold[1024];
    jstr(req, "model_dir", dir, sizeof dir);
    jstr(req, "ckpt", pre, sizeof pre);
    jstr(req, "dataset", hold, sizeof hold);
    int ngen = (int)jlong(req, "ngen", 6);
    if (!dir[0] || !pre[0] || !hold[0]) {
        return err_resp(resp, cap, "parametri apply mancanti (model_dir/ckpt/dataset)");
    }
    /* HOT-SWAP: ckpt = lista di prefissi separati da virgola -> scambia N adapter su un solo load. */
    if (strchr(pre, ',') != NULL) return lora_swap_run(dir, pre, hold, ngen, resp, cap);

    char bp[1100]; snprintf(bp,sizeof bp,"%s_best_B.raw",pre);
    /* schema: head-only (frozen all-but-head / layer -1, A[H,rank]/B[rank,VOCAB]) oppure
     * LoRA v_proj (layer>=0, A[VIN,rank]/B[rank,VOUT]). Parita' con EVAL. */
    int layer = DEFAULT_LAYER, ahead = 0;
    char metap[1100]; snprintf(metap,sizeof metap,"%s_best.meta",pre);
    FILE* fm=fopen(metap,"r");
    if(fm){ char lbuf[64];
        while(fgets(lbuf,sizeof lbuf,fm)){
            if(strncmp(lbuf,"layer=",6)==0){ int l=atoi(lbuf+6); layer=l; if(l<0) ahead=1; }
            else if(strncmp(lbuf,"frozen=",7)==0 && strstr(lbuf,"all-but-head")) ahead=1;
            else if(strncmp(lbuf,"mod=",4)==0){ int o=0; const char* q=lbuf+4;
                while(*q && *q!='\n' && *q!='\r' && o<31) g_lora_mod[o++]=*q++; g_lora_mod[o]=0; }
        }
        fclose(fm);
    }
    if (load_model_shape(dir, ahead ? DEFAULT_LAYER : layer) != 0) return err_resp(resp, cap, g_err);
    int in_dim = ahead ? H : VIN, out_dim = ahead ? VOCAB : VOUT;
    FILE* fb=fopen(bp,"rb"); if(!fb){ return err_resp(resp, cap, "open B"); }
    fseek(fb,0,SEEK_END); long sz=ftell(fb); fclose(fb);
    if(sz <= 0 || sz % (out_dim*4) != 0){ return err_resp(resp, cap, "B size non coerente"); }
    int rank=(int)(sz/(out_dim*4));
    char ap[1100]; snprintf(ap,sizeof ap,"%s_best_A.raw",pre);
    float* A=rd_raw(ap,in_dim*rank);
    float* B=rd_raw(bp,rank*out_dim);
    if (!A || !B) { free(A); free(B); return err_resp(resp, cap, g_err); }

    Win w;
    if (load_first_sample(hold,&w)!=0) { free(A); free(B); return err_resp(resp, cap, g_err); }

    LlmEngineLlmModel mod=E->load_model(dir);
    if(!mod){ free(A); free(B); return err_resp(resp, cap, "load_model"); }

    int32_t out1[16], out2[16];
    E->lora_clear(mod);
    E->lora_set_enabled(mod,0);
    gen(mod,w.ids,w.n,ngen,out1);
    if(E->lora_add(mod,ahead?HEAD_LAYER:layer,ahead?"lm_head":(g_lora_mod[0]?g_lora_mod:"v_proj"),rank,1.0f,A,B,in_dim,out_dim)!=0){ free(A);free(B); E->free_model(mod); return err_resp(resp, cap, "lora_add"); }
    E->lora_set_enabled(mod,1);
    gen(mod,w.ids,w.n,ngen,out2);

    int diff=0;
    for(int i=0;i<ngen;i++) diff += out1[i]!=out2[i];
    E->lora_clear(mod);
    E->free_model(mod);
    free(A); free(B);
    snprintf(resp, cap,
        "{\"type\":\"result\",\"data\":{\"ok\":true,\"ckpt\":\"%s_best\",\"layer\":%d,"
        "\"rank\":%d,\"demoChanged\":%d,\"message\":\"%s\"}}\n",
        pre, layer, rank, diff>0?1:0,
        diff>0 ? "l'adapter ricaricato cambia l'inferenza (persistenza OK)"
               : "nessun effetto (checkpoint vuoto o regola non appresa)");
    return 0;
}

/* ============================ DISTILL SAMPLE (Stream D, fase D1) ============================
 * Genera k risposte a temperatura temp per ogni prompt di un .jsonl e filtra con VERIFY,
 * scrivendo le coppie (prompt,risposta) accettate in `out`. Stessa infrastruttura del
 * fine-tune (in-process, motore MLX via ABI): il runtime C condiviso e il C# la chiamano. */

#define DISTILL_MAX_PROMPT 4096

/* estrae un campo stringa da una RIGA JSON (de-escape base, come gmb_distill.c). */
static int jstr_line(const char* line, const char* key, char* out, size_t cap)
{
    if (!line || !key || !out || cap == 0) return 0;
    char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(line, pat);
    if (!p) { out[0] = 0; return 0; }
    p = strchr(p + strlen(pat), ':'); if (!p) { out[0] = 0; return 0; }
    p = strchr(p, '"'); if (!p) { out[0] = 0; return 0; }
    p++;
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < cap) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                case 'r': out[o++] = '\r'; break;
                case '"': out[o++] = '"'; break;
                case '\\': out[o++] = '\\'; break;
                case '/': out[o++] = '/'; break;
                default: out[o++] = *p; break;
            }
        } else out[o++] = *p;
        p++;
    }
    out[o] = 0;
    return 1;
}

static void wjson_str(FILE* f, const char* s)
{
    fputc('"', f);
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
        else if (c == '\n') fputs("\\n", f);
        else if (c == '\t') fputs("\\t", f);
        else if (c == '\r') fputs("\\r", f);
        else fputc(c, f);
    }
    fputc('"', f);
}

/* filtro VERIFY: "rule" = risposta non vuota e, se il prompt ha answer/solution, lo contiene;
 * "code" NON e' supportato in nativo (errore esplicito, mai filtro silenzioso); "" = non vuota. */
static int distill_verify_ok(const char* verify, const char* resp_text, const char* answer)
{
    if (!resp_text || !*resp_text) return 0;
    if (verify && strcmp(verify, "rule") == 0 && answer && *answer)
        return strstr(resp_text, answer) != NULL;
    return 1;
}

static int gmb_ai_distill_sample_exec_impl(const char* req, char* resp, size_t cap)
{
    g_failed = 0; g_err[0] = 0;
    E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_IDS))
        return err_resp(resp, cap, "motore con IDS non disponibile per SAMPLE");
    char dir[1024], prompts[1024], out_path[1024], verify[32];
    jstr(req, "model_dir", dir, sizeof dir);
    jstr(req, "prompts", prompts, sizeof prompts);
    jstr(req, "out", out_path, sizeof out_path);
    jstr(req, "verify", verify, sizeof verify);
    int k = (int)jlong(req, "k", 4);
    double temp = jdbl(req, "temp", 0.7);
    int maxlen = (int)jlong(req, "maxlen", 256);
    unsigned seed = (unsigned)jlong(req, "seed", 20260909);
    if (!dir[0] || !prompts[0] || !out_path[0])
        return err_resp(resp, cap, "parametri sample mancanti (model_dir/prompts/out)");
    if (k < 1 || k > 64) return err_resp(resp, cap, "k fuori range [1,64]");
    if (maxlen < 1 || maxlen > 4096) return err_resp(resp, cap, "maxlen fuori range [1,4096]");
    if (verify[0] && strcmp(verify, "rule") != 0)
        return err_resp(resp, cap, "verify: solo 'rule' supportato in SAMPLE nativo");

    LlmEngineLlmModel mod = E->load_model(dir);
    if (!mod) return err_resp(resp, cap, "load_model");
    char tokpath[1100]; snprintf(tokpath, sizeof tokpath, "%s/tokenizer.json", dir);
    llmtoken_handle tk = llmtoken_load(tokpath);
    if (!tk) { E->free_model(mod); return err_resp(resp, cap, "tokenizer.json non caricabile"); }
    FILE* fi = fopen(prompts, "rb");
    if (!fi) { llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "prompts non apribile"); }
    FILE* fo = fopen(out_path, "wb");
    if (!fo) { fclose(fi); llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "out non scrivibile"); }

    char* line = (char*)malloc(65536);
    char* prompt = (char*)malloc(65536);
    char* answer = (char*)malloc(65536);
    char* txt = (char*)malloc(65536);
    int32_t* ids = (int32_t*)malloc(sizeof(int32_t) * DISTILL_MAX_PROMPT);
    int32_t* gids = (int32_t*)malloc(sizeof(int32_t) * (maxlen + 8));
    if (!line || !prompt || !answer || !txt || !ids || !gids) {
        free(line); free(prompt); free(answer); free(txt); free(ids); free(gids);
        fclose(fi); fclose(fo); llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "oom");
    }
    LlmEngineLlmSampling sp; memset(&sp, 0, sizeof sp);
    sp.temperature = (float)temp; sp.seed = (long long)seed;
    int npairs = 0, nfail = 0, ngen = 0, nskip = 0;
    while (fgets(line, 65536, fi)) {
        size_t ln = strlen(line);
        while (ln && (line[ln-1] == '\n' || line[ln-1] == '\r')) line[--ln] = 0;
        if (ln == 0) continue;
        int has = jstr_line(line, "prompt", prompt, 65536)
               || jstr_line(line, "problem", prompt, 65536)
               || jstr_line(line, "question", prompt, 65536);
        if (!has || !prompt[0]) { nskip++; continue; }
        answer[0] = 0;
        if (!jstr_line(line, "answer", answer, 65536)) jstr_line(line, "solution", answer, 65536);
        int ntok = llmtoken_encode(tk, prompt, ids, DISTILL_MAX_PROMPT);
        if (ntok <= 0) { nskip++; continue; }
        for (int i = 0; i < k; i++) {
            LlmEngineLlmSession s = E->create_session(mod);
            if (!s) { nfail++; ngen++; continue; }
            E->set_sampling(s, &sp);
            E->append_ids(s, ids, ntok);
            int ng = E->generate_ids(s, maxlen, -1, gids);
            E->destroy_session(s);
            ngen++;
            if (ng <= 0) { nfail++; continue; }
            int nb = llmtoken_decode(tk, gids, ng, txt, 65536);
            if (nb <= 0) { nfail++; continue; }
            if (distill_verify_ok(verify, txt, answer)) {
                fputs("{\"prompt\":", fo); wjson_str(fo, prompt);
                fputs(",\"response\":", fo); wjson_str(fo, txt);
                fputs("}\n", fo);
                npairs++;
            } else nfail++;
        }
    }
    fclose(fi); fclose(fo);
    free(line); free(prompt); free(answer); free(txt); free(ids); free(gids);
    llmtoken_free(tk);
    E->free_model(mod);
    snprintf(resp, cap,
        "{\"type\":\"result\",\"data\":{\"pairs\":%d,\"fail\":%d,\"gen\":%d,\"skip\":%d,\"out\":\"%s\"}}\n",
        npairs, nfail, ngen, nskip, out_path);
    return 0;
}

/* ============================ DISTILL EVAL (Stream D, fase D4) ============================
 * Valutazione paired: idx 0 = modello BASE, idx 1 = modello + adapter (schema LORA APPLY).
 * Per ogni prompt genera `repeat` risposte (k token, T=temp), misura i token mediani e il
 * pass-rate VERIFY. Scrive le metriche in `out` e nella response. §0bis: logica condivisa. */

#define DISTILL_MAX_EVAL_PROMPTS 512
typedef struct { int32_t* ids; int n; char ans[512]; } EvalPrompt;

static int cmp_int(const void* a, const void* b) { int x = *(const int*)a, y = *(const int*)b; return (x > y) - (x < y); }
static double median_int(int* v, int n) {
    if (n <= 0) return 0.0;
    qsort(v, (size_t)n, sizeof(int), cmp_int);
    return (n & 1) ? (double)v[n/2] : ((double)v[n/2-1] + (double)v[n/2]) / 2.0;
}

static double g_evaltok[2] = { -1, -1 }, g_evalpass[2] = { -1, -1 }, g_evalacc[2] = { -1, -1 };
double gmb_ai_distill_eval_tok(int idx) { return (idx >= 0 && idx < 2) ? g_evaltok[idx] : -1; }
double gmb_ai_distill_eval_pass(int idx) { return (idx >= 0 && idx < 2) ? g_evalpass[idx] : -1; }
double gmb_ai_distill_eval_acc(int idx) { return (idx >= 0 && idx < 2) ? g_evalacc[idx] : -1; }

static int gmb_ai_distill_eval_exec_impl(const char* req, char* resp, size_t cap)
{
    g_failed = 0; g_err[0] = 0;
    E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_IDS))
        return err_resp(resp, cap, "motore con IDS non disponibile per EVAL");
    char dir[1024], prompts[1024], ckpt[1024], out_path[1024], verify[32];
    jstr(req, "model_dir", dir, sizeof dir);
    jstr(req, "prompts", prompts, sizeof prompts);
    jstr(req, "ckpt", ckpt, sizeof ckpt);
    jstr(req, "out", out_path, sizeof out_path);
    jstr(req, "verify", verify, sizeof verify);
    int maxlen = (int)jlong(req, "maxlen", 256);
    int repeat = (int)jlong(req, "repeat", 3);
    double temp = jdbl(req, "temp", 0.7);
    unsigned seed = (unsigned)jlong(req, "seed", 20260909);
    if (!dir[0] || !prompts[0]) return err_resp(resp, cap, "parametri eval mancanti (model_dir/prompts)");
    if (maxlen < 1 || maxlen > 4096) return err_resp(resp, cap, "maxlen fuori range [1,4096]");
    if (repeat < 1 || repeat > 16) return err_resp(resp, cap, "repeat fuori range [1,16]");
    if (verify[0] && strcmp(verify, "rule") != 0) return err_resp(resp, cap, "verify: solo 'rule' supportato in EVAL nativo");

    LlmEngineLlmModel mod = E->load_model(dir);
    if (!mod) return err_resp(resp, cap, "load_model");
    char tokpath[1100]; snprintf(tokpath, sizeof tokpath, "%s/tokenizer.json", dir);
    llmtoken_handle tk = llmtoken_load(tokpath);
    if (!tk) { E->free_model(mod); return err_resp(resp, cap, "tokenizer.json non caricabile"); }

    /* adapter (idx 1) se richiesto — schema LORA APPLY: head (frozen all-but-head, layer -1,
     * A[H,rank]/B[rank,VOCAB]) oppure v_proj (layer>=0, A[VIN,rank]/B[rank,VOUT]). */
    int have_adapt = 0, alayer = DEFAULT_LAYER, arank = DEFAULT_RANK, ahead = 0;
    float* A = NULL; float* B = NULL;
    if (ckpt[0]) {
        char bp[1100]; snprintf(bp, sizeof bp, "%s_best_B.raw", ckpt);
        char mp[1100]; snprintf(mp, sizeof mp, "%s_best.meta", ckpt);
        FILE* fm = fopen(mp, "r");
        if (fm) {
            char lb[64];
            while (fgets(lb, sizeof lb, fm)) {
                if (strncmp(lb, "layer=", 6) == 0) { alayer = atoi(lb + 6); }
                else if (strncmp(lb, "frozen=", 7) == 0 && strstr(lb, "all-but-head")) { ahead = 1; }
                else if (strncmp(lb, "mod=", 4) == 0) { int o=0; const char* q=lb+4;
                    while(*q && *q!='\n' && *q!='\r' && o<31) g_lora_mod[o++]=*q++; g_lora_mod[o]=0; }
            }
            fclose(fm);
        }
        if (alayer < 0) ahead = 1;
        if (load_model_shape(dir, ahead ? DEFAULT_LAYER : alayer) != 0) { llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, g_err); }
        FILE* fb = fopen(bp, "rb");
        if (!fb) { llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "adapter B non apribile"); }
        fseek(fb, 0, SEEK_END); long sz = ftell(fb); fclose(fb);
        int in_dim = ahead ? H : VIN, out_dim = ahead ? VOCAB : VOUT;
        if (sz <= 0 || sz % (out_dim*4) != 0) { llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "B size non coerente"); }
        arank = (int)(sz/(out_dim*4));
        char ap[1100]; snprintf(ap, sizeof ap, "%s_best_A.raw", ckpt);
        A = rd_raw(ap, in_dim*arank); B = rd_raw(bp, arank*out_dim);
        if (!A || !B) { free(A); free(B); llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, g_err); }
        if (E->lora_add(mod, ahead ? HEAD_LAYER : alayer, ahead ? "lm_head" : (g_lora_mod[0]?g_lora_mod:"v_proj"), arank, 1.0f, A, B, in_dim, out_dim) != 0) {
            free(A); free(B); llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "lora_add adapter");
        }
        have_adapt = 1;
    }

    /* prompt in memoria (tokenizzati). */
    EvalPrompt* ep = (EvalPrompt*)calloc(DISTILL_MAX_EVAL_PROMPTS, sizeof(EvalPrompt));
    char* line = (char*)malloc(65536);
    char* pbuf = (char*)malloc(65536);
    char* etxt = (char*)malloc(65536);
    if (!ep || !line || !pbuf || !etxt) { free(ep); free(line); free(pbuf); free(etxt); free(A); free(B); llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "oom"); }
    FILE* fi = fopen(prompts, "rb");
    if (!fi) { free(ep); free(line); free(pbuf); free(etxt); free(A); free(B); llmtoken_free(tk); E->free_model(mod); return err_resp(resp, cap, "prompts non apribile"); }
    int npro = 0;
    while (npro < DISTILL_MAX_EVAL_PROMPTS && fgets(line, 65536, fi)) {
        size_t ln = strlen(line);
        while (ln && (line[ln-1] == '\n' || line[ln-1] == '\r')) line[--ln] = 0;
        if (ln == 0) continue;
        int has = jstr_line(line, "prompt", pbuf, 65536) || jstr_line(line, "problem", pbuf, 65536)
               || jstr_line(line, "question", pbuf, 65536);
        if (!has || !pbuf[0]) continue;
        ep[npro].ids = (int32_t*)malloc(sizeof(int32_t) * DISTILL_MAX_PROMPT);
        if (!ep[npro].ids) break;
        ep[npro].ans[0] = 0;
        if (!jstr_line(line, "answer", ep[npro].ans, sizeof ep[npro].ans)) jstr_line(line, "solution", ep[npro].ans, sizeof ep[npro].ans);
        int nt = llmtoken_encode(tk, pbuf, ep[npro].ids, DISTILL_MAX_PROMPT);
        if (nt <= 0) { free(ep[npro].ids); ep[npro].ids = NULL; continue; }
        ep[npro].n = nt;
        npro++;
    }
    fclose(fi);
    if (npro == 0) {
        for (int i = 0; i < DISTILL_MAX_EVAL_PROMPTS; i++) free(ep[i].ids);
        free(ep); free(line); free(pbuf); free(etxt); free(A); free(B); llmtoken_free(tk); E->free_model(mod);
        return err_resp(resp, cap, "nessun prompt valido in eval");
    }

    LlmEngineLlmSampling sp; memset(&sp, 0, sizeof sp);
    sp.temperature = (float)temp; sp.seed = (long long)seed;
    int32_t* gids = (int32_t*)malloc(sizeof(int32_t) * (maxlen + 8));
    int* toks = (int*)malloc(sizeof(int) * (size_t)npro * (size_t)repeat);
    if (!gids || !toks) { free(gids); free(toks); goto eval_cleanup; }

    for (int idx = 0; idx < 2; idx++) {
        if (idx == 1 && !have_adapt) { g_evaltok[1] = -1; g_evalpass[1] = -1; g_evalacc[1] = -1; continue; }
        E->lora_set_enabled(mod, idx == 1 ? 1 : 0);
        int nt = 0, pass = 0, acc_tot = 0, dr_tot = 0;
        for (int i = 0; i < npro; i++) {
            for (int r = 0; r < repeat; r++) {
                LlmEngineLlmSession s = E->create_session(mod);
                if (!s) { toks[nt++] = maxlen; continue; }
                E->set_sampling(s, &sp);
                E->append_ids(s, ep[i].ids, ep[i].n);
                llm_ai_mlx_session_build_ngram((llm_ai_mlx_session_handle)s, 3);
                int nacc = 0, ndr = 0;
                int ng = llm_ai_mlx_session_generate_ngram((llm_ai_mlx_session_handle)s, maxlen, -1, gids, 8);
                if (ng < 0) ng = E->generate_ids(s, maxlen, -1, gids);   /* fallback: generate normale */
                { long long _dr = 0, _ac = 0; if (llm_ai_mlx_session_spec_stats((llm_ai_mlx_session_handle)s, &_dr, &_ac, NULL) == 0) { ndr = (int)_dr; nacc = (int)_ac; } }
                acc_tot += nacc; dr_tot += ndr;
                E->destroy_session(s);
                if (ng < 0) ng = maxlen;
                toks[nt++] = ng;
                const char* txt = NULL;
                if (ng > 0 && llmtoken_decode(tk, gids, ng, etxt, 65536) > 0) txt = etxt;
                if (distill_verify_ok(verify, txt, ep[i].ans)) pass++;
            }
        }
        g_evaltok[idx] = median_int(toks, nt);
        g_evalpass[idx] = nt ? (double)pass / (double)nt : 0.0;
        g_evalacc[idx] = dr_tot ? (double)acc_tot / (double)dr_tot : -1;
    }

    if (out_path[0]) {
        FILE* fo = fopen(out_path, "wb");
        if (fo) {
            fprintf(fo, "{\"base\":{\"tok\":%.1f,\"pass\":%.4f},\"distill\":{\"tok\":%.1f,\"pass\":%.4f},"
                        "\"prompts\":%d,\"repeat\":%d}\n",
                    g_evaltok[0], g_evalpass[0], g_evaltok[1], g_evalpass[1], npro, repeat);
            fclose(fo);
        }
    }

eval_cleanup:
    for (int i = 0; i < DISTILL_MAX_EVAL_PROMPTS; i++) free(ep[i].ids);
    free(ep); free(line); free(pbuf); free(etxt); free(gids); free(toks); free(A); free(B);
    llmtoken_free(tk);
    E->lora_clear(mod); E->free_model(mod);
    if (g_failed) return err_resp(resp, cap, g_err);
    snprintf(resp, cap,
        "{\"type\":\"result\",\"data\":{\"baseTok\":%.1f,\"basePass\":%.4f,\"baseAcc\":%.4f,"
        "\"distTok\":%.1f,\"distPass\":%.4f,\"distAcc\":%.4f,\"prompts\":%d,\"repeat\":%d}}\n",
        g_evaltok[0], g_evalpass[0], g_evalacc[0], g_evaltok[1], g_evalpass[1], g_evalacc[1], npro, repeat);
    return 0;
}

/* ── Stack grande per le operazioni pesanti (training/generazione MLX) ───────────
 * Sui backend che chiamano via thread pool (es. PCode ParkBlocking) lo stack del thread
 * e' piccolo: il training del motore MLX lo saturava ("Stack overflow"). Eseguiamo il
 * lavoro su un thread dedicato con stack ampio, indipendentemente dal chiamante. */
typedef struct { int (*fn)(const char*, char*, size_t); const char* req; char* resp; size_t cap; int rc; } gmb_bigjob_t;
static void* gmb_bigjob_run(void* p) { gmb_bigjob_t* j = (gmb_bigjob_t*)p; j->rc = j->fn(j->req, j->resp, j->cap); return NULL; }
static int gmb_run_big_stack(int (*fn)(const char*, char*, size_t), const char* req, char* resp, size_t cap)
{
    pthread_attr_t at;
    if (pthread_attr_init(&at) != 0) return fn(req, resp, cap);
    pthread_attr_setstacksize(&at, 64u * 1024u * 1024u);
    gmb_bigjob_t j; j.fn = fn; j.req = req; j.resp = resp; j.cap = cap; j.rc = 0;
    pthread_t th;
    if (pthread_create(&th, &at, gmb_bigjob_run, &j) != 0) { pthread_attr_destroy(&at); return fn(req, resp, cap); }
    pthread_attr_destroy(&at);
    pthread_join(th, NULL);
    return j.rc;
}
int gmb_ai_finetune_exec(const char* req, char* resp, size_t cap) { return gmb_run_big_stack(gmb_ai_finetune_exec_impl, req, resp, cap); }
int gmb_ai_lora_apply_exec(const char* req, char* resp, size_t cap) { return gmb_run_big_stack(gmb_ai_lora_apply_exec_impl, req, resp, cap); }
int gmb_ai_distill_sample_exec(const char* req, char* resp, size_t cap) { return gmb_run_big_stack(gmb_ai_distill_sample_exec_impl, req, resp, cap); }
int gmb_ai_distill_eval_exec(const char* req, char* resp, size_t cap) { return gmb_run_big_stack(gmb_ai_distill_eval_exec_impl, req, resp, cap); }
int gmb_ai_distill_kl_exec(const char* req, char* resp, size_t cap) { return gmb_run_big_stack(gmb_ai_distill_kl_exec_impl, req, resp, cap); }
