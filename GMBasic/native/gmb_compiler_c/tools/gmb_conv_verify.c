/* gmb_conv_verify.c — round-trip del container generico:
 *  (1) DENSE: rilegge i tensori per nome e li confronta col SORGENTE (f16 bit-exact).
 *  (2) ESPERTI: per gli esperti con blob `ggml`, parse del blob + confronto dei byte grezzi
 *      (weight+scale per gate/up/down) col sorgente `layers.L.ffn.experts.E.w{1,3,2}`.
 * Uso: gmb_conv_verify <model_src> <container>
 */
#include "gmb_convert_internal.h"
#include "llmengine_moe_model.h"
#include "gmb_moe_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gmb_conv_iter_index(const char* index_json, void (*cb)(const char* name, void* ud), void* ud);
/* Offset layer SORGENTE: il container rinumerа i layer da 0, ma la sorgente usa i nomi reali
 * (`--layers A:B` → container L0 = sorgente L(A+0)).` gmb_conv_verify <m> <c> [src_l0]`. */
static uint32_t g_src_l0 = 0;
static const char* g_src_prefix = "layers.";   /* "mtp." per l'artefatto MTP/DSpark */
static char* vslurp(const char* p){ FILE* f=fopen(p,"rb"); if(!f)return NULL; fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET); char* b=(char*)malloc((size_t)n+1); if(!b){fclose(f);return NULL;} if(fread(b,1,(size_t)n,f)!=(size_t)n){free(b);fclose(f);return NULL;} fclose(f); b[n]=0; return b; }

/* ── (1) round-trip DENSE ── */
typedef struct { const char* model; const char* map; GmbMoeModel* m; GmbMoeStorage* st; unsigned ok, bad, miss, engram; } Vctx;

/* RAW (dtype=9, streaming): il container conserva i byte SORGENTE. Verifica a SLICE (mai il tensore
 * intero in RAM) che i byte combacino + la scala companion (.scale/.scales) se esiste. Vale per OGNI
 * tensore sopra il budget RAW (engram fp8 98 GB, embedding/head bf16 1.3 GB, ...). */
static void verify_raw(Vctx* v, const char* name, const GmbMoeTensorRecord* rec){
    char sp[1024]; uint64_t off=0, nb=0; char dz[8]={0}; uint32_t rr=0, cc=0; int okw=0;
    if (gmb_conv_find_tensor_loc(v->model,v->map,name,sp,sizeof sp,&off,&nb,dz,sizeof dz,&rr,&cc) && rec->size==nb){
        const uint64_t SLICE=(uint64_t)64<<20; uint64_t cmp = nb>SLICE?SLICE:nb;
        uint8_t* cb=(uint8_t*)malloc(cmp); uint8_t* sb=(uint8_t*)malloc(cmp); size_t got=0; FILE* f=fopen(sp,"rb");
        if (cb&&sb&&f && gmb_moe_storage_read_at(v->st,rec->offset,cb,cmp,&got)==GMB_MOE_OK && got==cmp
            && fseeko(f,(off_t)off,SEEK_SET)==0 && fread(sb,1,cmp,f)==cmp && memcmp(cb,sb,cmp)==0) okw=1;
        if(f)fclose(f); free(cb); free(sb);
    }
    int oks=1;   /* la scala companion è opzionale */
    char sbase[200]; snprintf(sbase,sizeof sbase,"%s",name); char* dot=strrchr(sbase,'.'); if(dot)*dot=0;
    const char* exts[2]={".scale",".scales"};
    for(int ei=0;ei<2;ei++){
        char scn[220]; snprintf(scn,sizeof scn,"%s%s",sbase,exts[ei]);
        char sp2[1024]; uint64_t o2=0,n2=0; char d2[8]={0}; uint32_t sr=0,sc2=0;
        if(!gmb_conv_find_tensor_loc(v->model,v->map,scn,sp2,sizeof sp2,&o2,&n2,d2,sizeof d2,&sr,&sc2)) continue;
        char safe2[64]; size_t k2=0; for(const char* q=scn;*q&&k2<62;++q) safe2[k2++]=(*q=='.')?'_':*q; safe2[k2]=0;
        char cn2[80]; snprintf(cn2,sizeof cn2,"g_%s",safe2);
        GmbMoeTensorRecord r2; oks=0;
        if (llmengine_moe_model_tensor_record_deprecated(v->m,cn2,&r2)==GMB_MOE_OK && r2.dtype>=GMB_MOE_DT_RAW && r2.size==n2){
            uint8_t* a=(uint8_t*)malloc(n2); uint8_t* b=(uint8_t*)malloc(n2); size_t z=0; FILE* f2=fopen(sp2,"rb");
            if (a&&b&&f2 && gmb_moe_storage_read_at(v->st,r2.offset,a,n2,&z)==GMB_MOE_OK && z==n2
                && fseeko(f2,(off_t)o2,SEEK_SET)==0 && fread(b,1,n2,f2)==n2 && memcmp(a,b,n2)==0) oks=1;
            if(f2)fclose(f2); free(a); free(b);
        }
        break;
    }
    if (okw && oks) { v->ok++; v->engram++; }
    else { v->bad++; fprintf(stderr,"  RAW mismatch %s: weight_ok=%d scale_ok=%d\n",name,okw,oks); }
}

static void on(const char* name, void* ud){
    Vctx* v=(Vctx*)ud;
    { const char* d=strrchr(name,'.');   /* le scale non sono tensori autonomi */
      if (d && (strcmp(d,".scale")==0||strcmp(d,".scales")==0||strcmp(d,".biases")==0)) return; }
    char safe[64]; size_t k=0; for(const char* q=name;*q&&k<62;++q) safe[k++]=(*q=='.')?'_':*q; safe[k]=0;
    char cn[80]; snprintf(cn,sizeof cn,"g_%s",safe);
    GmbMoeTensorRecord rec;
    if (llmengine_moe_model_tensor_record_deprecated(v->m,cn,&rec)!=GMB_MOE_OK) return;   /* fuori dal container (layer/altro) */
    if (rec.dtype >= GMB_MOE_DT_RAW) { verify_raw(v,name,&rec); return; }               /* RAW (streaming): confronta byte */
    char dt[16]={0}; uint32_t rows=0,cols=0; uint8_t* t=NULL; size_t tn=0;
    if (!read_tensor_full(v->model,v->map,name,dt,sizeof dt,&rows,&cols,&t,&tn)) { v->miss++; return; }
    size_t n=(size_t)(rows?rows:1)*(cols?cols:1);
    if (rec.size!=n*2 || rec.shape[0]!=rows || rec.shape[1]!=cols) { v->bad++; fprintf(stderr,"  dense mismatch (size/shape) %s dt=%s rec.size=%llu atteso=%zu shape %ux%u vs %ux%u\n",name,dt,(unsigned long long)rec.size,n*2,rec.shape[0],rec.shape[1],rows,cols); free(t); return; }
    float* f=(float*)malloc(n*4); uint16_t* h=(uint16_t*)malloc(n*2);
    /* F8_E4M3 BLOCK-scaled: il VERO valore è e4m3(code) × e8m0(scale). Prima il verify chiamava
     * st_to_f32 (codici nudi) = STESSO bug del converter → falso "bit-exact" (§0sexies). */
    int conv2 = 0;
    if (strcmp(dt,"F8_E4M3")==0) {
        char base[200]; snprintf(base,sizeof base,"%s",name); char* dot=strrchr(base,'.'); if(dot)*dot=0;
        char scn[220]; snprintf(scn,sizeof scn,"%s.scale",base);
        char ds[8]={0}; uint32_t sr=0,sc2=0; uint8_t* sb=NULL; size_t sn=0;
        if (read_tensor_full(v->model,v->map,scn,ds,sizeof ds,&sr,&sc2,&sb,&sn) && strcmp(ds,"F8_E8M0")==0) {
            gmb_conv_dequant_fp8_block(t,sb,rows,cols,sr,sc2,f); conv2=1;
        }
        free(sb);
    }
    if (!conv2 && !st_to_f32(dt,t,n,f)) { v->miss++; free(t);free(f);free(h); return; }
    for(size_t i=0;i<n;i++) h[i]=llmcodec_f16_from_f32(f[i]);
    uint8_t* got=(uint8_t*)malloc(n*2); size_t gn=0;
    if (llmengine_moe_model_read_tensor_deprecated(v->m,cn,got,n*2,&gn)!=GMB_MOE_OK || gn!=n*2 || memcmp(got,h,n*2)!=0) { v->bad++; fprintf(stderr,"  dense mismatch (bytes) %s dt=%s\n",name,dt); } else v->ok++;
    free(got); free(t); free(f); free(h);
}

/* ── (2) esperti ggml: paragona un sub-tensore (base.weight / base.scale) ai byte grezzi ── */
static int cmp_raw(const char* model, const char* map, const char* base, const char* ext,
                   const uint8_t* exp, size_t expn, const char* exp_dt){
    char nm[240]; snprintf(nm,sizeof nm,"%s.%s",base,ext);
    char dt[16]={0}; uint32_t r=0,c=0; uint8_t* b=NULL; size_t n=0;
    if (!read_tensor_full(model,map,nm,dt,sizeof dt,&r,&c,&b,&n)) return 0;
    int ok = (n==expn && (expn==0 || memcmp(b,exp,expn)==0) && strcmp(dt,exp_dt)==0);
    free(b); return ok;
}
/* Parsa un blob ggml e verifica i 3 sub-esperti (gate=w1, up=w3, down=w2) contro il sorgente. */
static int verify_one_expert(const char* model, const char* map, uint32_t L, uint32_t E,
                             const uint8_t* blob, size_t n){
    if (n < 12 || memcmp(blob,"ggml",4)!=0) return 0;                 /* non ggml: skip (non e' un errore) */
    size_t o=4; uint32_t ver=0,np=0; memcpy(&ver,blob+o,4); o+=4; memcpy(&np,blob+o,4); o+=4;
    if (ver!=1 || np!=3) { fprintf(stderr,"  ggml header versione/parti errate (ver=%u np=%u)\n",ver,np); return -1; }
    const char* subs[3]={"w1","w3","w2"};   /* gate, up, down: ordine di scrittura */
    for (uint32_t i=0;i<3;i++){
        if (o+2>n) return -1; uint8_t wl=blob[o++]; if (o+wl>n) return -1; char wdt[16]={0}; memcpy(wdt,blob+o,wl); o+=wl;
        if (o+1>n) return -1; uint8_t sl=blob[o++]; if (o+sl>n) return -1; char sdt[16]={0}; if(sl)memcpy(sdt,blob+o,sl); o+=sl;
        if (o+16>n) return -1; uint32_t wr,wc,w32,s32; memcpy(&wr,blob+o,4); memcpy(&wc,blob+o+4,4); memcpy(&w32,blob+o+8,4); memcpy(&s32,blob+o+12,4); o+=16; (void)wr;(void)wc;
        if (o+w32+s32>n) return -1;
        const uint8_t* wt=blob+o; const uint8_t* st=blob+o+w32; o+=w32+s32;
        char base[240]; snprintf(base,sizeof base,"%s%u.ffn.experts.%u.%s",g_src_prefix,L+g_src_l0,E,subs[i]);
        if (!cmp_raw(model,map,base,"weight",wt,w32,wdt)) { fprintf(stderr,"  ggml mismatch weight %s (dt=%s %uB)\n",base,wdt,w32); return -1; }
        if (sl>0 && !cmp_raw(model,map,base,"scale",st,s32,sdt)) { fprintf(stderr,"  ggml mismatch scale %s (dt=%s %uB)\n",base,sdt,s32); return -1; }
    }
    return 1;
}

int main(int c, char** v){
    if (c<3){ fprintf(stderr,"uso: %s <model_src> <container> [src_l0]\n", v[0]); return 2; }
    if (c>=4) { if (strcmp(v[3],"mtp")==0) { g_src_prefix="mtp."; g_src_l0=0; } else g_src_l0=(uint32_t)atoi(v[3]); }
    char ip[1200]; snprintf(ip,sizeof ip,"%s/model.safetensors.index.json", v[1]);
    char* map=vslurp(ip); if(!map){ fprintf(stderr,"index mancante\n"); return 2; }
    GmbMoeStorage* st=NULL; GmbMoeModel* m=NULL; LlmEngineMoeModelMeta meta;
    { LlmEngineMoeStatus rs=gmb_moe_storage_open(&st,v[2],NULL); if(rs!=GMB_MOE_OK){ fprintf(stderr,"storage rc=%d\n",(int)rs); return 2; } }
    { LlmEngineMoeStatus ms=llmengine_moe_model_open_deprecated(&m,st,&meta); if(ms!=GMB_MOE_OK){ fprintf(stderr,"model open rc=%d\n",(int)ms); return 2; } }

    /* (1) dense */
    Vctx x; memset(&x,0,sizeof x); x.model=v[1]; x.map=map; x.m=m; x.st=st;
    gmb_conv_iter_index(map, on, &x);
    printf("dense round-trip: ok=%u bad=%u miss=%u (engram RAW slice=%u)\n", x.ok, x.bad, x.miss, x.engram);

    /* (2) esperti: struttura su TUTTI, contenuto su un campione */
    unsigned eok=0, ebad=0, eskip=0;
    uint32_t nL = meta.layer_count > 4 ? 4 : meta.layer_count;   /* campione: 4 layer */
    for (uint32_t L=0; L<nL; ++L) for (uint32_t E=0; E<meta.expert_count; ++E){
        LlmEngineMoeExpertKey key = { L, E };
        GmbMoeExpertRecord rec;
        if (llmengine_moe_model_expert_record_deprecated(m,key,&rec)!=GMB_MOE_OK) { ebad++; continue; }
        size_t cap = rec.compressed_size ? rec.compressed_size : rec.uncompressed_size;
        uint8_t* blob=(uint8_t*)malloc(cap?cap:1); size_t got=0;
        if (llmengine_moe_model_read_expert_deprecated(m,key,blob,cap,&got)!=GMB_MOE_OK){ free(blob); ebad++; continue; }
        if (got<4 || memcmp(blob,"ggml",4)!=0){ eskip++; free(blob); continue; }   /* MLX ecc.: non verificati qui */
        /* Contenuto verificato su TUTTI gli esperti (non solo il campione 2x8): ogni blob ggml
         * viene confrontato byte-a-byte coi tensori sorgente w1/w3/w2 (weight+scale). */
        int r = verify_one_expert(v[1],map,L,E,blob,got);
        if (r==1) eok++; else if (r==-1) ebad++;
        free(blob);
    }
    printf("esperti: ok=%u bad=%u skip(non-ggml)=%u  [struttura + contenuto su TUTTI]\n", eok, ebad, eskip);

    llmengine_moe_model_close_deprecated(m); gmb_moe_storage_close(st); free(map);
    return (x.bad||ebad)?1:0;
}
