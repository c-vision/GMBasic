/* mock_registry.c — registry pacchetti finto per PackageManager (da mock_registry.py).
 * Uso: mock_registry --root DIR [--port 8087] [--log FILE] [--no-trace]
 * Endpoint: GET /api/packages/<nome>?…  e GET /download/<nome>/<versione>. ZIP store. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <zlib.h>
#include <strings.h>

static char ROOT[4096]; static FILE *LOG; static int TRACE=1;
static void tr(const char *fmt, const char *a, const char *b, const char *c) {
    char line[8192]; snprintf(line,sizeof line,fmt,a?a:"",b?b:"",c?c:"");
    if (TRACE) { fputs(line,stdout); fputc('\n',stdout); fflush(stdout); }
    if (LOG) { fputs(line,LOG); fputc('\n',LOG); fflush(LOG); }
}
typedef struct { unsigned char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, const void *p, size_t n) {
    if (b->n+n > b->cap) { size_t c=b->cap?b->cap:4096; while(c<b->n+n)c*=2; b->p=realloc(b->p,c); b->cap=c; }
    memcpy(b->p+b->n,p,n); b->n+=n;
}
static void put16(buf_t*b,uint16_t v){bput(b,&v,2);} static void put32(buf_t*b,uint32_t v){bput(b,&v,4);}
static int is_dir(const char*p){struct stat s;return stat(p,&s)==0&&S_ISDIR(s.st_mode);} static int is_file(const char*p){struct stat s;return stat(p,&s)==0&&S_ISREG(s.st_mode);}
static int cmpstr(const void*A,const void*B){return strcmp(*(char*const*)A,*(char*const*)B);}
static int versions(const char *name, buf_t *out, int *count) {
    char base[4096]; snprintf(base,sizeof base,"%s/%s",ROOT,name); DIR*d=opendir(base); if(!d)return 0;
    char **v=NULL; int n=0; struct dirent *de;
    while((de=readdir(d))){ if(de->d_name[0]=='.')continue; char mf[4096]; snprintf(mf,sizeof mf,"%s/%s/gmb-package.json",base,de->d_name); if(is_file(mf)){v=realloc(v,sizeof(char*)*(n+1));v[n++]=strdup(de->d_name);} }
    closedir(d); qsort(v,n,sizeof(char*),cmpstr);
    char q[8192]; int qn=snprintf(q,sizeof q,"{\"name\": \"%s\", \"versions\": {",name);
    for(int i=0;i<n;i++) qn+=snprintf(q+qn,sizeof q-qn,"%s\"%s\": {\"version\": \"%s\"}",i?", ":"",v[i],v[i]);
    qn+=snprintf(q+qn,sizeof q-qn,"}}"); bput(out,q,(size_t)qn); *count=n;
    for(int i=0;i<n;i++)free(v[i]); free(v); return 1;
}
typedef struct { char rel[2048]; char full[4096]; uint32_t crc,size,off; } zent_t;
static zent_t *Z; static int NZ, CZ;
static void collect(const char *dir,const char *rel) {
    DIR*d=opendir(dir); if(!d)return; struct dirent*de;
    while((de=readdir(d))){ if(de->d_name[0]=='.')continue; char f[4096],r[2048]; snprintf(f,sizeof f,"%s/%s",dir,de->d_name); snprintf(r,sizeof r,"%s%s%s",rel,rel[0]?"/":"",de->d_name);
        if(is_dir(f)) collect(f,r); else if(is_file(f)) {
            if(NZ==CZ){CZ=CZ?CZ*2:16;Z=realloc(Z,sizeof(zent_t)*CZ);} zent_t*z=&Z[NZ]; snprintf(z->rel,sizeof z->rel,"%s",r); snprintf(z->full,sizeof z->full,"%s",f);
            FILE*fp=fopen(f,"rb"); fseek(fp,0,SEEK_END); long n=ftell(fp); rewind(fp); unsigned char*b=malloc(n?n:1); fread(b,1,n,fp); fclose(fp); z->size=(uint32_t)n; z->crc=(uint32_t)crc32(0,b,z->size); free(b); NZ++;
        }
    } closedir(d);
}
static int zip_package(const char*name,const char*ver,buf_t*out){
    char src[4096]; snprintf(src,sizeof src,"%s/%s/%s",ROOT,name,ver); if(!is_dir(src))return 0;
    NZ=0; collect(src,""); buf_t cd={0}; size_t off=0;
    for(int i=0;i<NZ;i++){
        zent_t*z=&Z[i]; z->off=(uint32_t)out->n;
        FILE*fp=fopen(z->full,"rb"); fseek(fp,0,SEEK_END); long n=ftell(fp); rewind(fp); unsigned char*b=malloc(n?n:1); fread(b,1,n,fp); fclose(fp); z->size=(uint32_t)n; z->crc=(uint32_t)crc32(0,b,z->size);
        put32(out,0x04034b50); put16(out,20); put16(out,0); put16(out,0); put16(out,0); put16(out,0);
        put32(out,z->crc); put32(out,z->size); put32(out,z->size); put16(out,(uint16_t)strlen(z->rel)); put16(out,0); bput(out,z->rel,strlen(z->rel)); bput(out,b,z->size); free(b);
        put32(&cd,0x02014b50); put16(&cd,20); put16(&cd,20); put16(&cd,0); put16(&cd,0); put16(&cd,0); put16(&cd,0); put32(&cd,z->crc); put32(&cd,z->size); put32(&cd,z->size); put16(&cd,(uint16_t)strlen(z->rel)); put16(&cd,0); put16(&cd,0); put16(&cd,0); put16(&cd,0); put32(&cd,0); put32(&cd,z->off); bput(&cd,z->rel,strlen(z->rel));
        off=out->n; (void)off;
    }
    uint32_t cdoff=(uint32_t)out->n; bput(out,cd.p,cd.n); free(cd.p);
    put32(out,0x06054b50); put16(out,0); put16(out,0); put16(out,(uint16_t)NZ); put16(out,(uint16_t)NZ); put32(out,(uint32_t)cd.n); put32(out,cdoff); put16(out,0); return 1;
}
static void send_all(int fd,const char*status,const char*ctype,const void*body,size_t n){
    char h[512]; int hn=snprintf(h,sizeof h,"HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",status,ctype,n); send(fd,h,hn,0); if(n)send(fd,body,n,0);
}
static void handle(int fd){
    char req[8192]; ssize_t n=recv(fd,req,sizeof req-1,0); if(n<=0)return; req[n]=0; char method[16],path[4096]; sscanf(req,"%15s %4095s",method,path);
    tr("-> %s %s",method,path,NULL);
    char *qm=strchr(path,'?'); if(qm)*qm=0;
    if(strncmp(path,"/api/packages/",14)==0){ const char*name=path+14; buf_t b={0}; int c=0;
        if(versions(name,&b,&c)){ tr("   richiesta dettaglio: nome=%s versioni=%d",name,NULL,NULL); send_all(fd,"200 OK","application/json",b.p,b.n);} else { const char*j="{\"error\": \"package not found\"}"; send_all(fd,"404 Not Found","application/json",j,strlen(j)); } free(b.p); return; }
    if(strncmp(path,"/download/",10)==0){ char tmp[4096]; snprintf(tmp,sizeof tmp,"%s",path+10); char*sl=strrchr(tmp,'/'); if(!sl){send_all(fd,"404 Not Found","text/plain","",0);return;} *sl=0; const char*name=tmp,*ver=sl+1; buf_t z={0}; if(zip_package(name,ver,&z)){send_all(fd,"200 OK","application/zip",z.p,z.n); tr("   risposto 200 ZIP %s@%s",name,ver,NULL);} else send_all(fd,"404 Not Found","text/plain","",0); free(z.p); return; }
    send_all(fd,"404 Not Found","text/plain","",0);
}
int main(int argc,char**argv){ const char*log=NULL; int port=8087;
    for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"--root")&&i+1<argc)snprintf(ROOT,sizeof ROOT,"%s",argv[++i]); else if(!strcmp(argv[i],"--port")&&i+1<argc)port=atoi(argv[++i]); else if(!strcmp(argv[i],"--log")&&i+1<argc)log=argv[++i]; else if(!strcmp(argv[i],"--no-trace"))TRACE=0; }
    if(!ROOT[0]){fprintf(stderr,"uso: %s --root DIR [--port N] [--log FILE] [--no-trace]\n",argv[0]);return 2;}
    if(log){LOG=fopen(log,"a");}
    int s=socket(AF_INET,SOCK_STREAM,0); int one=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one); struct sockaddr_in a={0}; a.sin_family=AF_INET;a.sin_port=htons((uint16_t)port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); if(bind(s,(struct sockaddr*)&a,sizeof a)<0){perror("bind");return 2;} listen(s,16); tr("in ascolto su http://127.0.0.1:%d  pacchetti da %s",argv[0],ROOT,NULL);
    for(;;){int c=accept(s,NULL,NULL);if(c>=0){handle(c);close(c);}}
}
