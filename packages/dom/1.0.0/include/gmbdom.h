/* gmbdom.h — API del pacchetto `dom` (namespace DOM) su libxml2. */
#ifndef GMBMBOM_H
#define GMBMBOM_H
#ifdef __cplusplus
extern "C" {
#endif
int  gmbdom_available(void);
int  gmbdom_parse(const char* html);
char* gmbdom_text(int h, const char* xpath);
int  gmbdom_select(int h, const char* xpath);
int  gmbdom_numnodes(int nodeset);
char* gmbdom_nodeattr(int nodeset, int index, const char* attr);
int  gmbdom_freenodeset(int nodeset);
int  gmbdom_free(int h);
#ifdef __cplusplus
}
#endif
#endif
