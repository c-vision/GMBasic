' ============================================================================
' ReadGetDemo — sample del sistema @...GET / READ (Clipper) con le clausole
' WHEN / VALID / PICTURE / RANGE / MESSAGE / ERROR.
' ============================================================================
' Il READ è il loop unificato (modello CONTROLS, park-based): i campi @GET sono
' text-input; WHEN salta il campo se .F., VALID rivalida (re-focus se .F.),
' PICTURE/RANGE limitano l'input, MESSAGE/ERROR sono i messaggi. Esegue su
' IL/PCode/VM-C/C99.
'
' NOTA: con WHEN 0=1 i campi vengono saltati (nessun input → gira anche headless,
' come in questo sample). Con WHEN 1=1 i campi si leggono interattivamente.
'
' Esecuzione interattiva:  tools/gmb_c99native_run.sh samples/Controls/ReadGetDemo.bas
' ============================================================================
IMPORT CONSOLE

SUB Main()
  DIM nome AS STRING
  DIM eta  AS INTEGER

  PRINT "=== ReadGetDemo ==="

  ' @GET stringa: PICTURE maiuscolo, VALID richiede non-vuoto. WHEN 0=1 → saltato (headless).
  @ 3, 2 GET nome PICTURE "@!" WHEN 0 = 1 VALID LEN(nome) > 0 MESSAGE "Nome (obbligatorio)"

  ' @GET numerico: RANGE + VALID (maggiorenne), MESSAGE/ERROR. WHEN 0=1 → saltato (headless).
  @ 5, 2 GET eta RANGE 0, 120 WHEN 0 = 1 VALID eta >= 18 MESSAGE "Età 18..120" ERROR "Devi essere maggiorenne"

  READ

  PRINT "nome=[" + nome + "] eta=" + LTRIM(STR(eta))
  PRINT "=== fine ==="
END SUB
