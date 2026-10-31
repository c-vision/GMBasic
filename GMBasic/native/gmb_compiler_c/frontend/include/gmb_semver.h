/* gmb_semver.h — SemVer 2.0 + valutazione dei vincoli di versione (mondo C).
 *
 * PARITA' OBLIGATA con il lato C#: questo file replica, ramo per ramo,
 * `Parser.PackageManager.SemVer.Parse`, `ParseRange`, `SemVerPre.Compare` e
 * `Satisfies` (common/Parser/Parser.PackageManager.cs). Se una regola cambia da una
 * parte, cambia qui e li' NELLO STESSO COMMIT: due implementazioni che divergono
 * significano che lo stesso REQUIRE risolve versioni diverse a seconda del backend.
 *
 * PERCHE' UN MODULO SOLO: lo consumano tre front-end C distinti — il compilatore
 * bytecode (gmb_bytecode), il driver C99Native (gmb_c99native) e il REPL nativo.
 * Vive nel frontend gia' condiviso (frontend/src), non duplicato per consumer.
 */
#ifndef GMB_SEMVER_H
#define GMB_SEMVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_SEMVER_PRE_MAX 64   /* lunghezza max di un identificatore di prerelease */

typedef struct gmb_semver {
    int major;
    int minor;
    int patch;
    char prerelease[GMB_SEMVER_PRE_MAX]; /* "" = release */
    char build[32];                     /* metadati build: non influiscono sull'ordinamento */
} gmb_semver_t;

/* Parsa "1.2.3[-rc.1][+b5]"; accetta un 'v' iniziale. 1 = ok, 0 = non valida. */
int gmb_semver_parse(const char* version, gmb_semver_t* out);

/* Ordina come SemVer 2.0: major, minor, patch, poi prerelease (release > prerelease).
 * Restituisce <0, 0 o >0. */
int gmb_semver_cmp(const gmb_semver_t* a, const gmb_semver_t* b);

/* 1 se la versione ha un prerelease. */
int gmb_semver_is_prerelease(const gmb_semver_t* v);

/* Il vincolo e' soddisfatto? Linguaggio identico al C#:
 *   "" | "*" | "latest"  -> sempre
 *   "a || b"             -> almeno uno
 *   "^x" "~x"            -> come C# (x viene paddato a 3 componenti)
 *   "1.*" "1.2.*"        -> prefisso numerico, mai prerelease
 *   ">=x" ">x" "<=x" "<x"-> confronto (x paddato)
 *   altrimenti            -> versione esatta
 * 1 = soddisfatto, 0 = non soddisfatto o vincolo non interpretabile.
 *
 * ATTENZIONE: come in C#, un vincolo che NON e' un semver valido (es. un dist-tag come
 * "next") restituisce 0: la risoluzione dei tag passa dal registry e non da qui. */
int gmb_semver_satisfies(const gmb_semver_t* v, const char* constraint);

#ifdef __cplusplus
}
#endif

#endif /* GMB_SEMVER_H */