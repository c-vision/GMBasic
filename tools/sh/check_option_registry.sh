#!/usr/bin/env bash
# check_option_registry.sh — GATE: il MIRROR C# del registro opzioni deve essere IDENTICO al
# canonico C.
#
#   canonico (fonte) : GMBasic/native/gmb_runtime_c/src/gmb_opts.c        → g_opt_core[]
#   mirror (derivato): GMBasicSharp/common/IlBackendRuntime/IlRuntime.cs  → OptionCoreTable
#
# Confronta, NELL'ORDINE della tabella: numero di voci, nome di ogni voce, descrizione di ogni
# voce (testo esatto: finisce in `AI LIST OPTIONS`). Al primo scarto stampa il DIFF ESATTO della
# voce divergente — indice, riga di origine delle due fonti, stringa C e stringa C#, primo
# carattere diverso con il carattere dei due lati — e esce 1.
#
# PERCHÉ ESISTE. `AI LIST OPTIONS` è una superficie condivisa dalle 4 gambe (IL, PCodeVM, VM/C,
# C99Native) e il MiniTest `ai/ai_list_options` ne confronta il testo: una voce mancante o una
# descrizione diversa nel mirror managed fa divergere la parità. La voce `debug.memory` è già
# mancata DUE volte (2026-11-13 e 2026-11-14) e nessun controllo statico se ne accorgeva: il
# difetto si scopriva solo eseguendo la parità. Questo gate lo intercetta a costo zero.
#
# Uso:
#   tools/sh/check_option_registry.sh          # verifica completa
#   tools/sh/check_option_registry.sh --quiet  # stampa solo la divergenza (CI/gate)
#   tools/sh/check_option_registry.sh --c-file F --cs-file G   # prova il gate su COPIE
#
# Exit code: 0 = identiche · 1 = divergenza · 2 = fonte assente o tabella non estraibile.
#
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
C_FILE="$ROOT/GMBasic/native/gmb_runtime_c/src/gmb_opts.c"
CS_FILE="$ROOT/GMBasicSharp/common/IlBackendRuntime/IlRuntime.cs"
QUIET=0

while [ $# -gt 0 ]; do
    case "$1" in
        --quiet)   QUIET=1; shift ;;
        --c-file)  C_FILE="${2:-}"; shift 2 ;;
        --cs-file) CS_FILE="${2:-}"; shift 2 ;;
        -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
        *) echo "uso: $0 [--quiet] [--c-file F] [--cs-file G]" >&2; exit 2 ;;
    esac
done

[ -f "$C_FILE" ]  || { echo "sorgente canonico non trovato: $C_FILE" >&2; exit 2; }
[ -f "$CS_FILE" ] || { echo "mirror C# non trovato: $CS_FILE" >&2; exit 2; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/optreg.XXXXXX")" || exit 2
trap 'rm -rf "$WORK"' EXIT

# ── estrattore: una riga per voce, `<riga<TAB>nome<TAB>descrizione>` ─────────────────────────
# Le stringhe sono lette con un tokenizzatore che rispetta gli escape (`\"`, `\\`): una
# descrizione che contiene virgole o apici non spezza la voce. Una riga di voce che non produce
# DUE stringhe è un errore di estrazione (exit 3 di awk → exit 2 del gate): meglio fallire che
# dare un verde su una tabella letta a metà.
cat > "$WORK/extract.awk" <<'AWK'
function qs(s, arr,   i, n, inn, esc, ch, cur) {
    n = 0; inn = 0; esc = 0; cur = ""
    for (i = 1; i <= length(s); i++) {
        ch = substr(s, i, 1)
        if (!inn) { if (ch == "\"") { inn = 1; cur = "" }; continue }
        if (esc) { cur = cur "\\" ch; esc = 0; continue }
        if (ch == "\\") { esc = 1; continue }
        if (ch == "\"") { inn = 0; n++; arr[n] = cur; continue }
        cur = cur ch
    }
    return n
}
mode == "c"  && /g_opt_core\[\]/         { on = 1; next }
mode == "c"  && on && /^\};/             { on = 0 }
mode == "cs" && /OptionCoreTable *=/     { on = 1; next }
mode == "cs" && on && /^[[:space:]]*\];/ { on = 0 }
on {
    if (mode == "c") entry = ($0 ~ /^[[:space:]]*\{[[:space:]]*"/)
    else             entry = ($0 ~ /^[[:space:]]*\([[:space:]]*"/)
    if (!entry) next
    k = qs($0, a)
    if (k < 2) { printf("riga %d: voce non estraibile: %s\n", NR, $0) > "/dev/stderr"; bad = 1; next }
    printf("%d\t%s\t%s\n", NR, a[1], a[2])
}
END { if (bad) exit 3 }
AWK

# ── comparatore: nomi, ordine, descrizioni; diff esatto della voce divergente ─────────────────
cat > "$WORK/compare.awk" <<'AWK'
function firstdiff(a, b,   i, n, na, nb) {
    na = length(a); nb = length(b); n = (na < nb) ? na : nb
    for (i = 1; i <= n; i++) if (substr(a, i, 1) != substr(b, i, 1)) return i
    return (na == nb) ? 0 : n + 1
}
function chof(s, p) {
    if (p < 1 || p > length(s)) return "(fine stringa)"
    return "\047" substr(s, p, 1) "\047"
}
function fieldtxt(field, a, b, p,   s, lo) {
    s  = sprintf("      %-4s C : \"%s\"\n", field, a)
    s  = s sprintf("      %-4s C#: \"%s\"\n", field, b)
    s  = s sprintf("            primo carattere diverso: colonna %d - C=%s, C#=%s\n", p, chof(a, p), chof(b, p))
    lo = (p > 20) ? p - 20 : 1
    s  = s sprintf("            contesto C : ...%s\n", substr(a, lo, 50))
    s  = s sprintf("            contesto C#: ...%s\n", substr(b, lo, 50))
    return s
}
function entrytxt(a_name, b_name, a_desc, b_desc,   s) {
    s  = sprintf("      nome C : %s\n", (a_name == "(assente)" ? "(assente)" : "\"" a_name "\""))
    s  = s sprintf("      nome C#: %s\n", (b_name == "(assente)" ? "(assente)" : "\"" b_name "\""))
    s  = s sprintf("      desc C : %s\n", (a_desc == "(assente)" ? "(assente)" : "\"" a_desc "\""))
    s  = s sprintf("      desc C#: %s\n", (b_desc == "(assente)" ? "(assente)" : "\"" b_desc "\""))
    return s
}
BEGIN {
    n_c = 0
    while ((getline l < cfile) > 0) { split(l, f, "\t"); n_c++; cn[n_c] = f[2]; cd[n_c] = f[3]; cl[n_c] = f[1] }
    close(cfile)
    n_s = 0
    while ((getline l < csfile) > 0) { split(l, f, "\t"); n_s++; sn[n_s] = f[2]; sd[n_s] = f[3]; sl[n_s] = f[1] }
    close(csfile)

    max = (n_c > n_s) ? n_c : n_s
    ndiff = 0
    for (i = 1; i <= max; i++) {
        problem = ""
        if (i > n_c) {
            problem = sprintf("  voce #%d: presente SOLO nel mirror C# (%s:%s) - non esiste in g_opt_core[]",
                              i, cssrc, sl[i])
            a_name = "(assente)"; b_name = sn[i]; a_desc = "(assente)"; b_desc = sd[i]; p = 0
        } else if (i > n_s) {
            problem = sprintf("  voce #%d: MANCANTE nel mirror C# (%s:%s) - g_opt_core[] la dichiara: \"%s\"",
                              i, csrc, cl[i], cn[i])
            a_name = cn[i]; b_name = "(assente)"; a_desc = cd[i]; b_desc = "(assente)"; p = 0
        } else {
            dn = firstdiff(cn[i], sn[i])
            dd = firstdiff(cd[i], sd[i])
            if (dn == 0 && dd == 0) continue
            problem = sprintf("  voce #%d: %s - (%s:%s) vs (%s:%s)", i,
                              (dn != 0 ? "NOME divergente" : "DESCRIZIONE divergente"),
                              csrc, cl[i], cssrc, sl[i])
            a_name = cn[i]; b_name = sn[i]; a_desc = cd[i]; b_desc = sd[i]
            p = (dn != 0) ? dn : dd
            which = (dn != 0) ? "nome" : "desc"
        }
        ndiff++
        report = report problem "\n"
        if (p == 0) report = report entrytxt(a_name, b_name, a_desc, b_desc)
        else if (which == "nome") report = report fieldtxt("nome", a_name, b_name, p)
        else report = report fieldtxt("desc", a_desc, b_desc, p)
    }

    if (ndiff == 0) {
        if (quiet != "1")
            printf("registro opzioni: OK - %d voci, OptionCoreTable identica a g_opt_core[] (nomi, ordine, descrizioni)\n", n_c)
        exit 0
    }
    printf("DIVERGENZA REGISTRO OPZIONI - canonico %s (g_opt_core[]) vs mirror %s (OptionCoreTable)\n", csrc, cssrc)
    printf("  voci: C=%d, C#=%d - %d voce/he divergenti\n\n", n_c, n_s, ndiff)
    printf("%s\n", report)
    printf("Rimedio: allineare OptionCoreTable al testo canonico di g_opt_core[] (stesso ordine, stesso testo).\n")
    exit 1
}
AWK

awk -v mode=c  -f "$WORK/extract.awk" "$C_FILE"  > "$WORK/c.tsv"  2> "$WORK/c.err" || {
    echo "estrazione g_opt_core[] fallita ($C_FILE):" >&2; cat "$WORK/c.err" >&2; exit 2; }
awk -v mode=cs -f "$WORK/extract.awk" "$CS_FILE" > "$WORK/cs.tsv" 2> "$WORK/cs.err" || {
    echo "estrazione OptionCoreTable fallita ($CS_FILE):" >&2; cat "$WORK/cs.err" >&2; exit 2; }

nc="$(wc -l < "$WORK/c.tsv"  | tr -d ' ')"
ncs="$(wc -l < "$WORK/cs.tsv" | tr -d ' ')"
# Tabella vuota = estrazione rotta (il pattern non ha agganciato): mai un verde per assenza.
if [ "$nc" -eq 0 ] || [ "$ncs" -eq 0 ]; then
    echo "tabella vuota dopo l'estrazione (C=$nc, C#=$ncs): pattern non agganciato, gate inutilizzabile" >&2
    exit 2
fi

# Gli errori di estrazione parziale (righe non leggibili) sono già usciti sopra; un residuo qui
# significa tabella letta a metà: si blocca comunque.
if [ -s "$WORK/c.err" ] || [ -s "$WORK/cs.err" ]; then
    echo "estrazione parziale (righe non interpretabili):" >&2
    cat "$WORK/c.err" "$WORK/cs.err" >&2
    exit 2
fi

# niente `exec`: deve restare vivo il trap che cancella la cartella temporanea
awk -v cfile="$WORK/c.tsv" -v csfile="$WORK/cs.tsv" \
    -v csrc="${C_FILE#"$ROOT"/}" -v cssrc="${CS_FILE#"$ROOT"/}" \
    -v quiet="$QUIET" -f "$WORK/compare.awk"
exit $?
