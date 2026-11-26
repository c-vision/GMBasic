#!/usr/bin/env bash
# check_scene_negatives.sh — SUITE NEGATIVA SISTEMATICA delle superfici SCENE.
#
# PERCHÉ ESISTE. La revisione di SCENE aveva trovato una classe di difetto precisa: verbi
# con arità intermedie accettate (OBJECT a 4 argomenti, LAMP a 7-9) e argomenti fuori
# range/NaN/Inf che finivano nel kernel come no-op silenziosi. Il fix è nei parser, nell'emitter
# e nella facciata C; questo gate impedisce alla classe di rientrare: per OGNI verbo verifica
# che arità sbagliate, NaN/Inf e valori fuori range diano
#   - lo STESSO messaggio su c99n, IL, PCode e VM/C
#   - lo STESSO exit code (2 = rifiuto in compilazione, 6 = errore BASIC di runtime)
#
# Il confronto è sul messaggio CORE: l'harness toglie i prefissi delle gambe
#   ("[GMBASIC] ERROR 5: … (line N)", "BasicException: …", "BASIC Runtime Error: …",
#    "emissione: <file>:<line>: …") e la riga.
#
# C'è anche un caso di VALORE DI RITORNO (kind = pick): SCENE.PICK non solleva un errore,
# risponde un numero, quindi la sola arità non basta a proteggere le sue sentinelle. Il caso
# confronta il valore stampato (`PRINT "pick="; SCENE.PICK(x, y)`) e l'exit code su 4 gambe.
#
# Uso:
#   tools/sh/check_scene_negatives.sh                 # tutti i casi
#   tools/sh/check_scene_negatives.sh --filter VIEW   # sottoinsieme (nome/verbo)
#   tools/sh/check_scene_negatives.sh --list          # stampa i casi
#
# Prerequisiti: driver C99Native (`make gmbnative`), runner IL/PCode (`test_ilrun`) e
# VM/C (`test_vmrun`) costruiti. Nessun permesso di sweep: il gate è mirato al dominio SCENE.
#
# Su FAIL il .bas del caso e i log GREZZI delle quattro gambe sono copiati in una directory
# fuori da $WORK (che il trap cancella) e il percorso è stampato nel FAIL.
#
# Exit code: 0 = verde · 1 = divergenza reale delle aspettative · 3 = solo guasti
# d'infrastruttura (gamba morta senza eseguire il programma). Con entrambi vince 1.
set -uo pipefail

MONO="$(cd "$(dirname "$0")/../.." && pwd)"
C99RUN="$MONO/tools/sh/gmb_c99native_run.sh"
ILRUN="$MONO/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll"
VMRUN="$MONO/tools/runners/test_vmrun/bin/Debug/net10.0/test_vmrun.dll"

FILTER=""
LIST=0
while [ $# -gt 0 ]; do
  case "$1" in
    --filter) FILTER="${2:-}"; shift 2 ;;
    --list) LIST=1; shift ;;
    -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
    *) echo "argomento sconosciuto: $1" >&2; exit 2 ;;
  esac
done

for f in "$C99RUN" "$ILRUN" "$VMRUN"; do
  [ -f "$f" ] || { echo "manca $f (build non eseguita)" >&2; exit 2; }
done

# kind|nome|riga BASIC|atteso|exit atteso
# kind = arity (rifiuto in compilazione, exit 2) | value (errore di runtime, exit 6)
#        valid (deve arrivare in fondo senza messaggi) | pick (valore stampato da PRINT)
CASES=(
  # ── ARITÀ: ogni verbo, forma sbagliata (una in meno, una in più o intermedia) ──────────
  'arity~RESET-extra~SCENE RESET 1~SCENE RESET non accetta argomenti~2'
  'arity~VIEW-meno~SCENE VIEW 16, 16~SCENE VIEW w, h, fov~2'
  'arity~VIEW-piu~SCENE VIEW 16, 16, 60, 0~SCENE VIEW w, h, fov~2'
  'arity~TEXTURE-senza-argomenti~SCENE TEXTURE~SCENE TEXTURE celle | "file.png"~2'
  'arity~TEXTURE-piu~SCENE TEXTURE 8, 8~SCENE TEXTURE celle | "file.png"~2'
  'arity~OBJECT-4~SCENE OBJECT 1, 2, 3, 4~SCENE OBJECT r, g, b [, flags [, luce]]~2'
  'arity~OBJECT-6~SCENE OBJECT 1, 2, 3, 4, 5, 6~SCENE OBJECT r, g, b [, flags [, luce]]~2'
  'arity~VERTEX-4~SCENE OBJECT 0,0,0 : SCENE VERTEX 1, 2, 3, 4~SCENE VERTEX x, y, z, u, v [, nx, ny, nz]~2'
  'arity~VERTEX-6~SCENE OBJECT 0,0,0 : SCENE VERTEX 1, 2, 3, 4, 5, 6~SCENE VERTEX x, y, z, u, v [, nx, ny, nz]~2'
  'arity~QUAD-meno~SCENE QUAD 0, 1, 2~SCENE QUAD a, b, c, d~2'
  'arity~QUAD-piu~SCENE QUAD 0, 1, 2, 3, 4~SCENE QUAD a, b, c, d~2'
  'arity~BOX-meno~SCENE OBJECT 0,0,0 : SCENE BOX 0,0,0,1,1~SCENE BOX x0, y0, z0, x1, y1, z1~2'
  'arity~OBJPOS-meno~SCENE OBJPOS 0,0,0~SCENE OBJPOS x, y, z, yaw~2'
  'arity~CAMERA-meno~SCENE CAMERA 0,0,0,0~SCENE CAMERA x, y, z, yaw, pitch~2'
  'arity~LIGHT-meno~SCENE LIGHT 0, 1, 0~SCENE LIGHT dx, dy, dz, livello~2'
  'arity~SMOOTH-zero~SCENE SMOOTH~SCENE SMOOTH|ALPHA|TWOSIDED|AA on|off~2'
  'arity~ALPHA-zero~SCENE ALPHA~SCENE SMOOTH|ALPHA|TWOSIDED|AA on|off~2'
  'arity~TWOSIDED-zero~SCENE TWOSIDED~SCENE SMOOTH|ALPHA|TWOSIDED|AA on|off~2'
  'arity~AA-zero~SCENE AA~SCENE SMOOTH|ALPHA|TWOSIDED|AA on|off~2'
  'arity~LAMP-5~SCENE LAMP 0, 0,0,0, 1~SCENE LAMP n, x, y, z, raggio, quantità [, dx, dy, dz, angolo]~2'
  'arity~LAMP-7~SCENE LAMP 0, 0,0,0, 1, 1, 0~SCENE LAMP n, x, y, z, raggio, quantità [, dx, dy, dz, angolo]~2'
  'arity~FOG-zero~SCENE FOG~SCENE FOG distanza~2'
  'arity~FAR-zero~SCENE FAR~SCENE FAR distanza~2'
  'arity~SKY-meno~SCENE SKY 1~SCENE SKY repeat, shift~2'
  'arity~COLORMAP-extra~SCENE COLORMAP 1~SCENE COLORMAP non accetta argomenti~2'
  'arity~THING-meno~SCENE THING 0,0,0,0,1~SCENE THING n, x, y, z, w, h~2'
  'arity~PRESENT-extra~SCENE PRESENT 1~SCENE PRESENT non accetta argomenti~2'
  'arity~RENDER-extra~SCENE RENDER 1~SCENE RENDER non accetta argomenti~2'
  'arity~SAVE-zero~SCENE SAVE~SCENE SAVE: manca il percorso del PNG~2'
  'arity~PICK-zero~SCENE PICK~SCENE PICK on|off~2'
  'arity~PICK-extra~SCENE PICK on, off~SCENE PICK on|off~2'

  # ── VALORI: range, NaN e Inf per ogni verbo che li valida ──────────────────
  'value~VIEW-zero~SCENE VIEW 0, 16, 60~SCENE VIEW: w e h devono essere tra 1 e 8192~6'
  'value~VIEW-fov~SCENE VIEW 16, 16, 200~SCENE VIEW: fov deve essere tra 1 e 179 gradi~6'
  'value~VIEW-inf~SCENE VIEW 16, 16, 1e308*10~SCENE VIEW: fov deve essere un numero finito~6'
  'value~TEXTURE-zero~SCENE TEXTURE 0~SCENE TEXTURE: celle deve essere tra 1 e 64~6'
  'value~TEXTURE-65~SCENE TEXTURE 65~SCENE TEXTURE: celle deve essere tra 1 e 64~6'
  'value~VERTEX-noobj~SCENE VERTEX 0,0,0,0,0~SCENE VERTEX: nessun oggetto corrente (SCENE OBJECT prima)~6'
  'value~VERTEX-inf~SCENE OBJECT 0,0,0 : SCENE VERTEX 1e308*10,0,0,0,0~SCENE VERTEX: coordinate non finite~6'
  'value~QUAD-idx~SCENE OBJECT 0,0,0 : SCENE VERTEX 0,0,0,0,0 : SCENE QUAD 0,1,2,3~SCENE QUAD: indice di vertice fuori range~6'
  'value~BOX-noobj~SCENE BOX 0,0,0,1,1,1~SCENE BOX: serve un oggetto corrente (SCENE OBJECT prima)~6'
  'value~OBJPOS-noobj~SCENE OBJPOS 0,0,0,0~SCENE OBJPOS: serve un oggetto corrente (SCENE OBJECT prima)~6'
  'value~CAMERA-pitch~SCENE CAMERA 0,0,0,0,90~SCENE CAMERA: pitch tra -89 e 89 gradi~6'
  'value~LIGHT-zero~SCENE LIGHT 0,0,0,255~SCENE LIGHT: direzione nulla~6'
  'value~LIGHT-inf~SCENE LIGHT 1e308*10,0,0,255~SCENE LIGHT: direzione non finita~6'
  'value~LAMP-slot~SCENE LAMP 8,0,0,0,1,255~SCENE LAMP: lo slot deve essere tra 0 e 7~6'
  'value~LAMP-radius~SCENE LAMP 0,0,0,0,-1,255~SCENE LAMP: raggio non negativo e finito~6'
  'value~LAMP-inf~SCENE LAMP 0,1e308*10,0,0,1,255~SCENE LAMP: posizione non finita~6'
  'value~SPOT-dir~SCENE LAMP 0,0,0,0,1,255,0,0,0,30~SCENE LAMP SPOT: direzione nulla~6'
  'value~SPOT-angle~SCENE LAMP 0,0,0,0,1,255,0,0,1,0~SCENE LAMP SPOT: angolo tra 0 e 90 gradi~6'
  'value~FOG-neg~SCENE FOG -1~SCENE FOG: distanza non negativa e finita~6'
  'value~FOG-inf~SCENE FOG 1e308*10~SCENE FOG: distanza non negativa e finita~6'
  'value~FAR-neg~SCENE FAR -1~SCENE FAR: distanza non negativa e finita~6'
  'value~SKY-notex~SCENE SKY 1, 0~SCENE SKY: nessuna texture (SCENE TEXTURE prima di SKY)~6'
  'value~THING-idx~SCENE THING 300,0,0,0,1,1~SCENE THING: indice tra 0 e 255~6'
  'value~THING-w~SCENE THING 0,0,0,0,0,1~SCENE THING: w e h positivi e finiti~6'
  'value~THING-inf~SCENE THING 0,0,0,0,1,1e308*10~SCENE THING: w e h positivi e finiti~6'
  'value~RENDER-notarget~SCENE RENDER~SCENE RENDER: nessun target (SCENE VIEW prima)~6'
  'value~SAVE-noframe~SCENE SAVE "x.png"~SCENE SAVE: nessun frame (manca VIEW o RENDER)~6'
  'value~PRESENT-noframe~SCENE PRESENT~SCENE PRESENT: nessun frame (manca VIEW o RENDER)~6'
  'value~FOG-nan~SCENE FOG (1e308*10 - 1e308*10)~SCENE FOG: distanza non negativa e finita~6'

  # ── VALORI DI RITORNO: SCENE.PICK risponde un numero invece di sollevare, quindi il
  # difetto da fermare non è un messaggio ma una SENTINELLA SBAGLIATA. Le due sentinelle
  # vogliono dire cose diverse e non si confondono: -1 = assente/fuori (senza target, come
  # SCENE.PIXEL), -2 = target presente ma buffer spento. Il 2026-11-26 senza target tornava
  # -2. Qui si confronta il valore stampato e l'exit 0 su C99Native, IL, PCode e VM/C.
  'pick~PICK-notarget~PRINT "pick="; SCENE.PICK(1, 1)~-1~0'
  'pick~PICK-off~SCENE VIEW 16, 16, 60 : PRINT "pick="; SCENE.PICK(1, 1)~-2~0'

  # ── CONTROLLO POSITIVO: la stessa forma con ':' (separatore di statement) e tutti i
  # valori leciti deve arrivare in fondo su 4 gambe, exit 0 e nessun messaggio. Impedisce
  # che l'arità esatta o i controlli di range diventino troppo severi.
  'valid~VALID-colon~SCENE VIEW 16, 16, 60 : SCENE OBJECT 10,20,30 : SCENE VERTEX 0.2,-2,0,0,1 : SCENE VERTEX 4,-2,0,1,1 : SCENE VERTEX 4,2,0,1,0 : SCENE VERTEX 0.2,2,0,0,0 : SCENE QUAD 0,3,2,1 : SCENE CAMERA 0,0,-3,0,0 : SCENE RENDER~~0'
)

if [ "$LIST" = 1 ]; then
  for c in "${CASES[@]}"; do IFS='~' read -r kind name src msg rc <<<"$c"; echo "$kind $name :: $msg (exit $rc)"; done
  exit 0
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmb_scene_neg.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

# Messaggio core di una gamba: prefissi dei quattro formati tolti, riga tolta.
norm() {
  sed -nE \
    -e 's/^.*\[GMBASIC\] ERROR [0-9]+: (.*) \(line [0-9]+\)$/\1/p' \
    -e 's/^.*BasicException: (.*)$/\1/p' \
    -e 's/^.*Runtime Error: (.*)$/\1/p' \
    -e 's/^.*emissione: .*:[0-9]+: (.*)$/\1/p' \
    "$1" | head -1
}

# Valore stampato da un caso `pick`: la riga `pick=<numero>` (spazi finali tollerati).
pickval() {
  sed -nE 's/^pick=(-?[0-9]+) *$/\1/p' "$1" | tail -1
}

# Ultime righe non vuote del log di una gamba. Serve quando una gamba muore senza produrre un
# messaggio BASIC: l'harness (`test_vmrun`) scrive il motivo SOLO su stderr e in forme che
# `norm()` non riconosce (`[vm/c] rebuild FALLITO …`, `build GMBASIC.Conformance FALLITA:`, …).
# Senza questo il FAIL direbbe soltanto "messaggio vuoto".
rawtail() {
  grep -v '^[[:space:]]*$' "$1" | tail -2 | tr '\n' ' ' | sed 's/ *$//'
}

# Firme dell'HARNESS (non del programma BASIC) nel log di una gamba. Quando compaiono e la
# gamba non ha prodotto il messaggio atteso, la gamba è morta PRIMA di eseguire il caso:
# è un guasto d'infrastruttura, non una divergenza delle aspettative.
#
# Fonti, e SOLO messaggi fatali (un avviso informativo non deve mai classificare come
# infrastruttura un fallimento vero che viene dopo):
#   test_vmrun          `[vm/c] …` (VmHostFreshness), `build GMBASIC.Conformance FALLITA`,
#                       `GMBASIC.Conformance.dll non trovata`, `build del conformance non
#                       avviata`, `impossibile avviare il vmtest`, `file non trovato:`,
#                       `root GMBasic non trovata`
#   gmb_c99native_run   `driver assente: …`, `[c99native] suite 3D: compilazione di …`,
#                       `[c99native] runtime <rid> ASSENTE`, i quattro `[c99native] --static:`,
#                       `[c99native] ERRORE: link riuscito ma eseguibile INCOMPLETO`
#   test_ilrun          `[ILGEN] …` (exit 3), `[LOAD] …` (exit 4), `[DBG] metodo …` (exit 5),
#                       `[RUN] GeneratedProgram.Main non trovato` (exit 5) — le gambe IL e PCode
#                       muoiono prima di eseguire il caso, sempre su stderr e senza prefisso
#                       riconosciuto da `norm()`. Solo per [RUN] il testo è ristretto al metodo
#                       mancante: vedi sotto, le altre due righe [RUN] sono un errore BASIC.
#
# DELIBERATAMENTE FUORI (restano divergenze reali: l'ambiguità si risolve verso il sospetto,
# non verso il silenzio):
#   - `driver fallito:` — può essere il driver C che muore (build in corso) MA anche un difetto
#     dell'emitter sul C generato: non si può decidere dal testo;
#   - gli errori di `cc`/`ld` sul `.c` GENERATO (`error:`, `Undefined symbols`) — sono difetti
#     dell'emitter, non dell'infrastruttura;
#   - `[COMPILE] …` (test_ilrun, exit 2) — il BASIC è stato rifiutato o l'emitter è fallito: è un
#     difetto nostro, e per i casi di arità è esattamente il punto che il gate misura;
#   - `[RUN] exit code N` (PCode) e `[RUN] <Tipo>: <msg>` (IL) — sono il percorso NORMALE di un
#     errore BASIC di runtime (exit 6): la gamba ha ESEGUITO il programma. Un `[RUN] ` nudo in
#     INFRA_RE classificherebbe come infrastruttura ogni divergenza di messaggio sui casi
#     `value`, cioè proprio quelli che il gate deve vedere. Resta firma di morte solo
#     `[RUN] GeneratedProgram.Main non trovato`;
#   - `[DBG] key iniettata: …` — avviso informativo, non una morte (la firma è `[DBG] metodo `);
#   - `[OK] terminato senza eccezioni` — esito verde;
#   - `gmb_link_set:` — lo stesso prefisso copre sia `die()` sia l'avviso `terze parti assenti`
#     che NON ferma nulla;
#   - gli avvisi `[c99native]` che possono precedere un successo (`statica mancante…`,
#     `TRAY: … assente`, `dyld_info assente`, `bundle ->`, `libs.txt ->`, `cross … prodotto`).
# Il TIMEOUT (exit 124) NON è qui: il programma è partito e non è finito, e un blocco è un
# difetto del codice, non dell'harness.
# test_ilrun: quattro firme, tutte su stderr e tutte PRIMA dell'esecuzione del caso.
#   [ILGEN] — `IlProgramCompiler.Compile` ha lanciato o non ha prodotto l'assembly (exit 3);
#   [LOAD]  — l'assembly non si carica (exit 4);
#   [DBG]   — `--define`/GAI_METHOD chiede un metodo che non esiste (exit 5);
#   [RUN]   — l'assembly non ha `GeneratedProgram.Main` (exit 5). SOLO questa forma di [RUN]:
#             le altre due righe [RUN] del runner sono un errore BASIC di runtime (exit 6).
INFRA_RE='\[vm/c\] |build GMBASIC\.Conformance FALLITA|GMBASIC\.Conformance\.dll non trovata|build del conformance non avviata|impossibile avviare il vmtest|file non trovato:|root GMBasic non trovata|driver assente: |\[c99native\] suite 3D: compilazione di|\[c99native\] runtime [a-z0-9-]+ ASSENTE|\[c99native\] --static: (runtime per|manca|link fallito|il binario NON risulta statico)|\[c99native\] ERRORE: link riuscito ma eseguibile INCOMPLETO|\[ILGEN\] |\[LOAD\] |\[DBG\] metodo |\[RUN\] GeneratedProgram\.Main non trovato'

# Conserva una divergenza FUORI da $WORK: il trap cancella $WORK all'uscita, e un FAIL
# transiente (gamba che muore per un rebuild dell'host in corso) senza i log grezzi non è
# analizzabile a posteriori — né subito, né il giorno dopo. Si copiano il .bas del caso e i
# log di TUTTE e quattro le gambe: senza i log delle gambe che hanno risposto correttamente
# non si distingue "questa gamba è rotta" da "le altre hanno un comportamento diverso".
FAILROOT=""
keep_failure() {   # $1 nome caso, $2 riga BASIC, $3 exit atteso, $4 msg atteso, $5 ottenuto
  if [ -z "$FAILROOT" ]; then
    # TMPDIR su macOS termina con '/': senza toglierlo il path stampato conterrebbe '//'.
    local base="${TMPDIR:-/tmp}"; base="${base%/}"
    FAILROOT="$base/gmb_scene_neg_failures/$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$FAILROOT" || { echo "!! impossibile creare $FAILROOT: i log resteranno solo in $WORK" >&2; FAILROOT=""; return 0; }
  fi
  mkdir -p "$FAILROOT/$1" || return 0
  cp "$WORK/$1/$1.bas" "$FAILROOT/$1/" 2>/dev/null
  for i in 0 1 2 3; do cp "${LEG_LOG[$i]}" "$FAILROOT/$1/${LEG_NAME[$i]//\//_}.log" 2>/dev/null; done
  { echo "caso: $1"; echo "riga BASIC: $2"; echo "atteso: exit=$3 | $4"; echo "ottenuto:$5"; } \
    > "$FAILROOT/$1/esito.txt" 2>/dev/null
}

pass=0; fail=0; skip=0; infra_fail=0; real_fail=0
for c in "${CASES[@]}"; do
  IFS='~' read -r kind name src msg exp_rc <<<"$c"
  if [ -n "$FILTER" ] && [[ "$c" != *"$FILTER"* ]]; then skip=$((skip+1)); continue; fi
  # Una directory PER CASO: il runner compila il progetto della CWD, quindi più
  # .bas nella stessa cartella si sommavano (errore "WEEKDAY già dichiarato" e
  # risultati non più isolati).
  d="$WORK/$name"; mkdir -p "$d"; f="$d/$name.bas"
  printf 'IMPORT SCENE\n%s\n' "$src" > "$f"

  # Le quattro gambe in fila: LEG_NAME[i], il log LEG_LOG[i], l'exit LEG_RC[i], l'osservato
  # LEG_MSG[i]. Aggiungere una gamba = una riga + una voce di nome, non otto confronti.
  LEG_NAME=(c99 IL PCode "VM/C")
  LEG_LOG=("$WORK/leg1.log" "$WORK/leg2.log" "$WORK/leg3.log" "$WORK/leg4.log")
  LEG_RC=(); LEG_MSG=()
  ( cd "$d" && bash "$C99RUN" "$f" >"${LEG_LOG[0]}" 2>&1 </dev/null ); LEG_RC+=("$?")
  ( cd "$d" && dotnet "$ILRUN" --engine=il "$f" >"${LEG_LOG[1]}" 2>&1 </dev/null ); LEG_RC+=("$?")
  ( cd "$d" && dotnet "$ILRUN" --engine=pcode "$f" >"${LEG_LOG[2]}" 2>&1 </dev/null ); LEG_RC+=("$?")
  ( cd "$d" && dotnet "$VMRUN" "$f" >"${LEG_LOG[3]}" 2>&1 </dev/null ); LEG_RC+=("$?")
  # Un caso pick osserva il VALORE stampato; gli altri il messaggio core d'errore.
  for i in 0 1 2 3; do
    if [ "$kind" = pick ]; then LEG_MSG+=("$(pickval "${LEG_LOG[$i]}")")
    else LEG_MSG+=("$(norm "${LEG_LOG[$i]}")"); fi
  done

  # Confronto per gamba: l'exit code e l'osservato di OGNI gamba contro l'atteso, e il
  # dettaglio di chi ha divergito finisce in `why`, con il nome della gamba.
  ok=1; why=""; infra_leg=0; real_leg=0
  for i in 0 1 2 3; do
    leg_ok=1
    [ "${LEG_RC[$i]}" = "$exp_rc" ] || leg_ok=0
    [ "${LEG_MSG[$i]}" = "$msg" ] || leg_ok=0
    [ "$leg_ok" = 1 ] && continue
    ok=0
    # Gamba morta PRIMA di eseguire il programma (rebuild di gmb_vm_host, build del
    # conformance, runner non avviabile): il mismatch c'è, ma il log porta la firma
    # dell'harness. Non è una divergenza delle aspettative.
    if grep -qE "$INFRA_RE" "${LEG_LOG[$i]}"; then
      infra_leg=1
      why+=" ${LEG_NAME[$i]} GUASTO INFRASTRUTTURA exit=${LEG_RC[$i]}(atteso $exp_rc), gamba morta prima di eseguire: $(rawtail "${LEG_LOG[$i]}")"
    else
      real_leg=1
      if [ "${LEG_RC[$i]}" = 124 ]; then why+=" ${LEG_NAME[$i]} TIMEOUT exit=124(atteso $exp_rc)"
      elif [ "${LEG_RC[$i]}" != "$exp_rc" ]; then why+=" ${LEG_NAME[$i]} exit=${LEG_RC[$i]}(atteso $exp_rc)"; fi
      if [ "${LEG_MSG[$i]}" != "$msg" ]; then
        if [ -z "${LEG_MSG[$i]}" ]; then
          # Nessun messaggio BASIC e nessuna firma d'harness: la gamba ha eseguito ma non ha
          # parlato (una divergenza vera, non un guasto da build).
          why+=" ${LEG_NAME[$i]} NESSUN MESSAGGIO (gamba senza output BASIC) → $(rawtail "${LEG_LOG[$i]}")"
        else why+=" ${LEG_NAME[$i]} msg='${LEG_MSG[$i]}'"; fi
      fi
    fi
  done

  if [ "$ok" = 1 ]; then
    echo "PASS $kind/$name :: exit=$exp_rc | $msg"
    pass=$((pass+1))
  else
    keep_failure "$name" "$src" "$exp_rc" "$msg" "$why"
    echo "FAIL $kind/$name ($src)"
    echo "     atteso: exit=$exp_rc | $msg"
    echo "     ottenuto:$why"
    [ -n "$FAILROOT" ] && echo "     log completi: $FAILROOT/$name/"
    if [ "$real_leg" = 1 ]; then real_fail=$((real_fail+1))
    else
      infra_fail=$((infra_fail+1))
      echo "     ⚠ guasto d'infrastruttura: la gamba non ha eseguito il programma (non è una divergenza delle aspettative)"
    fi
    fail=$((fail+1))
  fi
done

echo "---------------------------------"
echo "SCENE negative: PASS=$pass FAIL=$fail SKIP=$skip"
[ -n "$FAILROOT" ] && echo "log dei fallimenti conservati: $FAILROOT"
[ "$infra_fail" -gt 0 ] && echo "guasti d'infrastruttura: $infra_fail caso/i su $fail FAIL (la gamba non ha eseguito il programma)"
# Contratto per la CI: 0 verde, 1 divergenza reale, 3 solo guasti d'infrastruttura.
# Con entrambi vince 1: una regressione vera non va nascosta dietro un flake.
if [ "$fail" -eq 0 ]; then exit 0; fi
if [ "$real_fail" -eq 0 ]; then exit 3; fi
exit 1
