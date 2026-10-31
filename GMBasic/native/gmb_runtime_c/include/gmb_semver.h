/* ============================================================================
 * gmb_semver.h — SemVer 2.0 + range matching (fonte di riferimento del Package
 * Manager, §"il backend di riferimento è C"). Il C# (Parser.PackageManager)
 * DEVE delegare qui via P/Invoke, non tenere una copia divergente.
 *
 * Parità 1:1 con la semantica C# (Parser.PackageManager.SemVer): parse
 * (TrimStart 'v'/' ', build '+…', prerelease '-…', 3 parti, validità ident),
 * compare (precedenza prerelease), satisfies ("*"/""/"latest", "||", "^", "~",
 * "1.*"/"1.2.*", ">=", ">", "<=", "<", esatto), find_best (più alta che
 * soddisfa TUTTE le constraints).
 * ============================================================================ */
#ifndef GMB_SEMVER_H
#define GMB_SEMVER_H

#include "gmb_runtime.h"

/* Versione SemVer 2.0. `has_pre` = prerelease presente; build è ignorata dal
 * confronto ma inclusa nell'uguaglianza "esatta". */
typedef struct {
    int  major, minor, patch;
    int  has_pre;
    char prerelease[128];
    char build[128];
} gmb_semver_t;

/* Parsa "v1.2.3-rc.1+b5". Ritorna 1 se valido, 0 altrimenti (out azzerato). */
int gmb_semver_parse(const char* s, gmb_semver_t* out);

/* Normalizza un range con 1-2 componenti ("1.0"→"1.0.0"). Ritorna 1/0. */
int gmb_semver_parse_range(const char* s, gmb_semver_t* out);

/* Precedenza SemVer 2.0 (build ignorata): <0, 0, >0. */
int gmb_semver_cmp(const gmb_semver_t* a, const gmb_semver_t* b);

/* Uguaglianza "esatta" (include build) — usata dal vincolo esatto. */
int gmb_semver_eq(const gmb_semver_t* a, const gmb_semver_t* b);

/* Ritorna 1 se `v` soddisfa il range (stessa semantica del resolver C#). */
int gmb_semver_satisfies(const gmb_semver_t* v, const char* range);

/* Formatta in "M.m.p[-pre][+build]" in `out` (cap). Ritorna 1/0. */
int gmb_semver_format(const gmb_semver_t* v, char* out, size_t cap);

/* Sceglie la versione PIÙ ALTA (per precedenza) tra `versions` che soddisfa
 * TUTTE le `constraints` (AND; ogni constraint gestisce "||"). Scrive la scelta
 * in `out` (cap). Ritorna 1 se trovata, 0 altrimenti. */
int gmb_semver_find_best(int n_constraints, const char* const* constraints,
                         int n_versions, const char* const* versions,
                         char* out, size_t cap);

#endif /* GMB_SEMVER_H */
