/* ============================================================================
 * gmb_pkg_install.h — materializzazione a install-time (riferimento C del PM):
 * sorgente esterna (source github-release), copia esempi, client-build.
 * Il C# lo consuma via P/Invoke (dopo la risoluzione).
 * ============================================================================ */
#ifndef GMB_PKG_INSTALL_H
#define GMB_PKG_INSTALL_H

#include "gmb_runtime.h"

/* Materializza la sorgente esterna (source github-release) se dichiarata e mancante. 1 ok, 0 errore. */
int gmb_pkg_ensure_source(const char* install_dir, const char* platform_key);

/* Copia gli header dichiarati in `source.headers` dall'asset estratto in `extract_dir` dentro
   <install_dir>/include/ (contratto §3.4). Ritorna il numero di header copiati (0 = nessuno).
   Esposta perché testabile SENZA rete: gmb_pkg_ensure_source la chiama dopo il download. */
int gmb_pkg_copy_source_headers(const char* install_dir, const char* extract_dir);

/* Copia gli esempi del pacchetto in <project_root>/samples/packages/<name>@<version>/. 1 ok.
   project_root = root del progetto consumatore, PASSATA ESPLICITAMENTE dal chiamante (mai
   dedotta dalla CWD: la CWD può trovarsi dentro il sorgente di un pacchetto che contiene una
   propria cartella samples/ → destinazione sbagliata). NULL/vuoto → fallback che risale dalla
   CWD cercando GMBASIC.sln (marcatore di progetto vero), NON una generica samples/. */
int gmb_pkg_copy_samples(const char* install_dir, const char* project_root);

/* Client-build: compila il C se manca il precompilato per la piattaforma.
   1 = compilato, 0 = nulla da fare, -1 = errore. */
int gmb_pkg_ensure_build(const char* install_dir, const char* platform_key);

/* Publisher-build (`gmb pkg build`): esegue il build dichiarato + verifica gli output. 1 ok, 0 errore. */
int gmb_pkg_run_build(const char* install_dir, const char* platform_key);

#endif /* GMB_PKG_INSTALL_H */
