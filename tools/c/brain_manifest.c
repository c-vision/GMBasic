/* brain_manifest.c — manifest versionato degli artefatti rag/ e graphs/
 * (da brain_manifest.py). Uso: brain_manifest [repo_root] [--check] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sqlite3.h>
#include <json-c/json.h>
#include <openssl/sha.h>

static int ends(const char *s, const char *x) { size_t a=strlen(s),b=strlen(x); return a>=b && !strcmp(s+a-b,x); }
static int cmpstr(const void *a, const void *b) { return strcmp(*(char*const*)a, *(char*const*)b); }
static void sha256_file(const char *p, char out[71]) {
    unsigned char h[SHA256_DIGEST_LENGTH]; SHA256_CTX c; SHA256_Init(&c);
    FILE *f=fopen(p,"rb"); if(!f){ out[0]=0; return; }
    unsigned char b[1<<16]; size_t n;
    while((n=fread(b,1,sizeof b,f))>0) SHA256_Update(&c,b,n);
    fclose(f); SHA256_Final(h,&c);
    strcpy(out,"sha256:");
    for(int i=0;i<SHA256_DIGEST_LENGTH;i++) sprintf(out+7+i*2,"%02x",h[i]);
}
static void utc_mtime(const char *p, char out[32]) {
    struct stat st; if(stat(p,&st)!=0){strcpy(out,"");return;}
    struct tm t; gmtime_r(&st.st_mtime,&t); strftime(out,32,"%Y-%m-%dT%H:%M:%SZ",&t);
}
static json_object *rag_info(const char *path) {
    json_object *o=json_object_new_object(); sqlite3 *db=NULL;
    if(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)!=SQLITE_OK) {
        json_object_object_add(o,"error",json_object_new_string(db?sqlite3_errmsg(db):"sqlite"));
        if(db) sqlite3_close(db); return o;
    }
    const char *tables[3]={"vec_documents","vec_chunks","vec_embeddings"};
    const char *keys[3]={"documents","chunks","embeddings"};
    for(int i=0;i<3;i++){
        char q[128]; snprintf(q,sizeof q,"select count(*) from %s",tables[i]);
        sqlite3_stmt *st=NULL; int64_t v=0; int ok=sqlite3_prepare_v2(db,q,-1,&st,NULL)==SQLITE_OK && sqlite3_step(st)==SQLITE_ROW;
        if(ok) v=sqlite3_column_int64(st,0);
        if(st) sqlite3_finalize(st);
        json_object_object_add(o,keys[i], ok?json_object_new_int64(v):NULL);
    }
    { sqlite3_stmt *st=NULL; int64_t v=0; int ok=sqlite3_prepare_v2(db,"select count(*) from fts_chunks",-1,&st,NULL)==SQLITE_OK && sqlite3_step(st)==SQLITE_ROW;
      if(ok)v=sqlite3_column_int64(st,0); if(st)sqlite3_finalize(st);
      json_object_object_add(o,"fts_rows",ok?json_object_new_int64(v):NULL); }
    { sqlite3_stmt *st=NULL; int64_t v=0; int ok=sqlite3_prepare_v2(db,"select coalesce(max(dimensions),0) from vec_embeddings",-1,&st,NULL)==SQLITE_OK && sqlite3_step(st)==SQLITE_ROW;
      if(ok)v=sqlite3_column_int64(st,0); if(st)sqlite3_finalize(st);
      if(ok) json_object_object_add(o,"dimensions",json_object_new_int64(v)); }
    { sqlite3_stmt *st=NULL; int ok=sqlite3_prepare_v2(db,"select name from vec_collections",-1,&st,NULL)==SQLITE_OK;
      if(ok){ json_object *arr=json_object_new_array(); while(sqlite3_step(st)==SQLITE_ROW){const unsigned char*n=sqlite3_column_text(st,0); if(n)json_object_array_add(arr,json_object_new_string((const char*)n));} json_object_object_add(o,"collections",arr);} if(st)sqlite3_finalize(st); }
    sqlite3_close(db); return o;
}
static json_object *graph_info(const char *path) {
    json_object *o=json_object_new_object(); FILE *f=fopen(path,"rb");
    if(!f){json_object_object_add(o,"error",json_object_new_string("open"));return o;}
    fseek(f,0,SEEK_END); long n=ftell(f); rewind(f); char *b=malloc(n+1);
    if(!b||fread(b,1,n,f)!=(size_t)n){free(b);fclose(f);json_object_object_add(o,"error",json_object_new_string("read"));return o;}
    fclose(f); b[n]=0; json_object *r=json_tokener_parse(b); free(b);
    if(!r){json_object_object_add(o,"error",json_object_new_string("json"));return o;}
    const char *ks[3]={"nodes","edges","dangling"}; const char *ok[3]={"nodes","edges","dangling"};
    for(int i=0;i<3;i++){json_object *a=NULL; if(json_object_object_get_ex(r,ks[i],&a)&&json_object_is_type(a,json_type_array)) json_object_object_add(o,ok[i],json_object_new_int((int)json_object_array_length(a))); else json_object_object_add(o,ok[i],json_object_new_int(0));}
    json_object_put(r); return o;
}
static void add_entry(json_object *arr, const char *kind, const char *name, const char *rel, const char *full, json_object *extra) {
    json_object *e=json_object_new_object(); struct stat st; stat(full,&st);
    char sha[71], mt[32]; sha256_file(full,sha); utc_mtime(full,mt);
    json_object_object_add(e,"kind",json_object_new_string(kind));
    json_object_object_add(e,"name",json_object_new_string(name));
    json_object_object_add(e,"path",json_object_new_string(rel));
    json_object_object_add(e,"bytes",json_object_new_int64((int64_t)st.st_size));
    json_object_object_add(e,"mtime",json_object_new_string(mt));
    json_object_object_add(e,"sha256",json_object_new_string(sha));
    json_object_object_foreach(extra,k,v){ json_object_get(v); json_object_object_add(e,k,v); }
    json_object_array_add(arr,e); json_object_put(extra);
}
int main(int argc, char **argv) {
    const char *root="."; int check=0;
    for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"--check"))check=1; else root=argv[i]; }
    char rag[4096],graphs[4096],man[4096]; snprintf(rag,sizeof rag,"%s/rag",root); snprintf(graphs,sizeof graphs,"%s/graphs",root); snprintf(man,sizeof man,"%s/rag/MANIFEST.json",root);
    json_object *arts=json_object_new_array();
    DIR *d=opendir(rag);
    if(d){ char **ns=NULL; int n=0; struct dirent *de; while((de=readdir(d))) if(ends(de->d_name,".db")){ns=realloc(ns,sizeof(char*)*(n+1));ns[n++]=strdup(de->d_name);} closedir(d); qsort(ns,n,sizeof(char*),cmpstr);
        for(int i=0;i<n;i++){ char full[4096],rel[4096]; snprintf(full,sizeof full,"%s/%s",rag,ns[i]); snprintf(rel,sizeof rel,"rag/%s",ns[i]); char name[1024]; snprintf(name,sizeof name,"%.*s",(int)(strlen(ns[i])-3),ns[i]); add_entry(arts,"rag",name,rel,full,rag_info(full)); free(ns[i]); } free(ns); }
    d=opendir(graphs);
    if(d){ char **ns=NULL; int n=0; struct dirent *de; while((de=readdir(d))){ const char *x=de->d_name; if(ends(x,".json")&&!ends(x,".stats.json")&&!ends(x,".state.json")&&strcmp(x,"MANIFEST.json")){ns=realloc(ns,sizeof(char*)*(n+1));ns[n++]=strdup(x);} } closedir(d); qsort(ns,n,sizeof(char*),cmpstr);
        for(int i=0;i<n;i++){ char full[4096],rel[4096]; snprintf(full,sizeof full,"%s/%s",graphs,ns[i]); snprintf(rel,sizeof rel,"graphs/%s",ns[i]); char name[1024]; snprintf(name,sizeof name,"%.*s",(int)(strlen(ns[i])-5),ns[i]); add_entry(arts,"graph",name,rel,full,graph_info(full)); free(ns[i]); } free(ns); }
    int count=json_object_array_length(arts);
    if(check){ printf("OK: %d artefatti presenti\n",count); json_object_put(arts); return 0; }
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts); struct tm t; gmtime_r(&ts.tv_sec,&t); char now[32]; strftime(now,sizeof now,"%Y-%m-%dT%H:%M:%SZ",&t);
    json_object *m=json_object_new_object();
    json_object_object_add(m,"generated_at",json_object_new_string(now));
    json_object_object_add(m,"repo",json_object_new_string(root));
    json_object_object_add(m,"note",json_object_new_string("Manifest VERSIONATO degli artefatti dei cervelli (rag/ e graphs/ sono gitignored). Rigenerare con: brain_manifest"));
    json_object_object_add(m,"artifacts",arts);
    FILE *f=fopen(man,"w"); if(!f){perror(man);json_object_put(m);return 2;}
    fputs(json_object_to_json_string_ext(m,JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE),f); fputc('\n',f); fclose(f); json_object_put(m);
    printf("scritto %s: %d artefatti\n",man,count);
    return 0;
}
