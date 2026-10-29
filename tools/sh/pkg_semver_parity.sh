#!/usr/bin/env bash
# pkg_semver_parity.sh — PARITA' delle due implementazioni SemVer del mondo C.
#
# PERCHE' ESISTE. La selezione della versione di un pacchetto avviene in DUE mondi che, per
# architettura, non possono linkarsi a vicenda:
#   * `native/gmb_compiler_c/frontend/src/gmb_semver.c` — usata dal COMPILATORE
#     (`gmb_pkg_collect_requires` in gmb_bytecode) per risolvere i REQUIRE;
#   * `native/gmb_runtime_c/src/gmb_semver.c` — usata dal RESOLVER/PM (gmb_pkg_resolver).
# Il frontend dichiara esplicitamente «Nessuna dipendenza dal runtime» (frontend/CMakeLists.txt)
# e il runtime referenzia il compilatore (gmb_compile_bas_file, weak): unificarli con un link
# creerebbe un CICLO. Quindi la sorgente resta doppia e la garanzia deve essere il COMPORTAMENTO.
#
# COSA FA. Compila lo stesso probe (corpus di versioni × vincoli × confronti) una volta contro
# ciascuna implementazione e confronta l'output riga per riga. Qualsiasi divergenza = exit 1.
# Ha già trovato un bug reale: il resolver confrontava anche il build metadata, quindi
# "1.2.3+b5" NON soddisfaceva "1.2.3" (npm/SemVer 2.0 §10 lo ignorano).
#
# USO: tools/sh/pkg_semver_parity.sh [-q]
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../GMBasic" && pwd)"
# Repo autonomo (GMBasic) oppure monorepo (GMBasicMonoRepo, con GMBasic/ dentro).
if [ ! -d "$ROOT/native/gmb_runtime_c" ] && [ -d "$ROOT/GMBasic/native/gmb_runtime_c" ]; then
  ROOT="$ROOT/GMBasic"
fi

FE_INC="$ROOT/native/gmb_compiler_c/frontend/include"
FE_SRC="$ROOT/native/gmb_compiler_c/frontend/src/gmb_semver.c"
RT_INC="$ROOT/native/gmb_runtime_c/include"
RT_SRC="$ROOT/native/gmb_runtime_c/src/gmb_semver.c"

for f in "$FE_SRC" "$RT_SRC" "$FE_INC/gmb_semver.h" "$RT_INC/gmb_semver.h"; do
  [ -f "$f" ] || { echo "pkg_semver_parity: manca $f" >&2; exit 2; }
done

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmbsemver.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/probe.c" <<'PROBE'
/* Corpus: versioni "difficili" (prerelease, build, leading zero, padding, multi-cifra)
 * × vincoli (range, OR, wildcard, operatori, dist-tag) × confronti a coppie. */
#include "gmb_semver.h"
#include <stdio.h>

static const char* VERS[] = {
    "1.0.0", "1.2.3", "1.2.3-rc.1", "2.0.0", "0.9.0", "v1.2.3", "1.2",
    "1.2.3+b5", "01.2.3", "1.2.3-01", "1.2.3-0a", "1.10.0", "1.5.10", "1.5.6",
    "3.0.0-beta.2", "1.0.0-alpha.beta", "1.0.0-alpha.1", "0.0.0", "10.20.30"
};
static const char* CONS[] = {
    "*", "latest", "", "^1.0.0", "^0.9.0", "~1.2", "1.*", "1.2.*", "1.2.3",
    "=1.2.3", "1.2.3 || 2.0.0", "^1.0.0-rc.1", ">1.0.0", ">=1.0.0", "<=2.0.0",
    "<2.0.0", ">1.5.5", "~1.2.3", "^1.2.3", "1.2.1", "2", "1.0.0 - 2.0.0", "next"
};

int main(void)
{
    int nv = (int)(sizeof VERS / sizeof VERS[0]);
    int nc = (int)(sizeof CONS / sizeof CONS[0]);
    for (int i = 0; i < nv; i++) {
        gmb_semver_t v;
        int ok = gmb_semver_parse(VERS[i], &v);
        printf("parse %-18s -> %d", VERS[i], ok);
        if (ok) printf(" [%d.%d.%d pre='%s']", v.major, v.minor, v.patch, v.prerelease);
        printf("\n");
    }
    for (int i = 0; i < nv; i++) {
        gmb_semver_t v;
        if (!gmb_semver_parse(VERS[i], &v)) continue;
        for (int k = 0; k < nc; k++)
            printf("sat %-18s %-19s -> %d\n", VERS[i], CONS[k], gmb_semver_satisfies(&v, CONS[k]));
    }
    for (int a = 0; a < nv; a++) {
        gmb_semver_t va;
        if (!gmb_semver_parse(VERS[a], &va)) continue;
        for (int b = 0; b < nv; b++) {
            gmb_semver_t vb;
            if (!gmb_semver_parse(VERS[b], &vb)) continue;
            printf("cmp %-18s %-18s -> %d\n", VERS[a], VERS[b], gmb_semver_cmp(&va, &vb));
        }
    }
    return 0;
}
PROBE

cc -O0 -I"$FE_INC" "$WORK/probe.c" "$FE_SRC" -o "$WORK/fe" || { echo "pkg_semver_parity: build frontend fallita" >&2; exit 2; }
cc -O0 -I"$RT_INC" "$WORK/probe.c" "$RT_SRC" -o "$WORK/rt" || { echo "pkg_semver_parity: build runtime fallita" >&2; exit 2; }

"$WORK/fe" > "$WORK/out_fe.txt"
"$WORK/rt" > "$WORK/out_rt.txt"

LINES=$(wc -l < "$WORK/out_fe.txt" | tr -d ' ')

if ! diff -u "$WORK/out_fe.txt" "$WORK/out_rt.txt" > "$WORK/diff.txt"; then
  N=$(grep -c '^[+-][^+-]' "$WORK/diff.txt")
  echo "pkg_semver_parity: DIVERGENZA tra frontend e runtime ($N righe su $LINES)" >&2
  head -40 "$WORK/diff.txt" >&2
  exit 1
fi

# SemVer 2.0 §10: il build metadata NON influisce sul matching (comportamento npm).
# È il caso che aveva fatto divergere le due copie: lo si verifica qui in modo esplicito,
# così una regressione è nominata invece di essere solo "una riga di diff".
if ! grep -Eq "^sat +1\.2\.3\+b5 +1\.2\.3 +-> 1$" "$WORK/out_rt.txt"; then
  echo "pkg_semver_parity: '1.2.3+b5' non soddisfa '1.2.3' (build metadata deve essere ignorato)" >&2
  exit 1
fi

[ "${1:-}" = "-q" ] || echo "pkg_semver_parity: OK — frontend e runtime identici su $LINES righe (corpus + build metadata)"
exit 0
