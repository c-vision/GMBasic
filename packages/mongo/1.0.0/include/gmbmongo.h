/* gmbmongo.h — API del pacchetto `mongo` (namespace MONGO). */
#ifndef GMBMONGO_H
#define GMBMONGO_H

#ifdef __cplusplus
extern "C" {
#endif

/* 1 = la libreria è presente. */
int gmbmongo_available(void);
/* Tenta una connessione TCP reale all'URL "mongodb://host:porta/...". 0 = non raggiungibile. */
int gmbmongo_connect(const char* url);
/* 0 = server non verificato (nessun handshake MongoDB in questo shim). */
int gmbmongo_ping(int handle);
int gmbmongo_close(int handle);

#ifdef __cplusplus
}
#endif
#endif /* GMBMONGO_H */
