#!/usr/bin/env bash
# ui_popup_check.sh — GATE: i POPUP non devono lasciare pixel sporchi.
#
# Apre e chiude una dropdown e un context menu (tools/basic/ui_popup_check.bas) e
# confronta con gmbimgdiff la pagina pulita (pop0) con quella dopo i due cicli
# (pop3): devono essere IDENTICHE. Verifica anche che il popup APERTO sia diverso
# dalla pagina pulita (altrimenti il test non sta provando nulla).
#
# Uso: bash tools/sh/ui_popup_check.sh      (esce 1 se il gate e' rosso)
set -u
cd "$(dirname "$0")/../.." || exit 1

REPO="$PWD"
DIFF="$REPO/GMBasic/native/gmb_runtime_c/build/gmbimgdiff"
[ -x "$DIFF" ] || cmake --build GMBasic/native/gmb_runtime_c/build --target gmbimgdiff -j4 >/dev/null 2>&1
[ -x "$DIFF" ] || { echo "ui_popup_check: gmbimgdiff non disponibile"; exit 1; }

WORK="$REPO/tools/basic/work"
mkdir -p "$WORK"
rm -f "$WORK"/pop0.png "$WORK"/pop1.png "$WORK"/pop2.png "$WORK"/pop3.png

SDL_VIDEODRIVER=dummy bash "$REPO/tools/sh/gmb_c99native_run.sh" "$REPO/tools/basic/ui_popup_check.bas" popchk "" \
    >"$WORK/popup.log" 2>&1
if ! grep -q "POPUP-CHECK-OK" "$WORK/popup.log"; then
    echo "ui_popup_check: esecuzione fallita (vedi tools/basic/work/popup.log)"
    tail -5 "$WORK/popup.log"
    exit 1
fi

fail=0
# 1) il popup APERTO deve cambiare la pagina (altrimenti il test e' vuoto)
if "$DIFF" "$WORK/pop0.png" "$WORK/pop1.png" >/dev/null 2>&1; then
    echo "ROSSO  dropdown: aprire la lista NON cambia l'immagine (popup non disegnato)"
    fail=1
else
    echo "OK     dropdown: la lista aperta e' diversa dalla pagina pulita"
fi

# 2) dopo aver chiuso tutto la pagina deve tornare IDENTICA
if "$DIFF" "$WORK/pop0.png" "$WORK/pop3.png" >/dev/null 2>&1; then
    echo "OK     popup: pagina identica dopo apri/chiudi (nessun pixel residuo)"
else
    echo "ROSSO  popup: la pagina NON torna identica dopo apri/chiudi"
    "$DIFF" "$WORK/pop0.png" "$WORK/pop3.png" 2>&1 | head -1
    fail=1
fi

rm -f "$WORK"/pop0.png "$WORK"/pop1.png "$WORK"/pop2.png "$WORK"/pop3.png
exit $fail
