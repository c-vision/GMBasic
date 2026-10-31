/* gmb_pkg_glob.h — match glob per i path del pacchetto (riferimento C del PM).
 * Semantica 1:1 con PackageBuilder.GlobMatch (C#): '*' = un segmento, '**' = qualsiasi,
 * '?' = un carattere non-'/'. Case-insensitive. Un pattern SENZA '/' matcha anche il
 * basename; un pattern-cartella matcha il prefisso (r == p o r inizia con p + '/'). */
#ifndef GMB_PKG_GLOB_H
#define GMB_PKG_GLOB_H

#include "gmb_runtime.h"

/* 1 se relPath matcha il pattern, 0 altrimenti. */
int gmb_pkg_glob_match(const char* pattern, const char* relPath);

#endif /* GMB_PKG_GLOB_H */
