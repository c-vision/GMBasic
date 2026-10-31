' MiniTest — divisione per zero: `/`, `\` e MOD sollevano ERR 11 sulle 4 gambe.
'
' Divergenza trovata portando make_lora_dataset.py in BASIC: la stessa espressione dava quattro
' esiti diversi. Con un handler attivo (ON ERROR o TRY/CATCH):
'   IL     : `\` → ERR 6 (overflow di Convert.ToInt64 su infinito), `/`/MOD → valore non finito
'            (l'errore compariva solo stampandolo: STR$ di Inf/NaN lanciava)
'   PCodeVM: `/` → +Infinity, `\` → +Infinity, MOD → NaN, nessun errore
'   VM/C   : tutti e tre → 0 in silenzio
'   C99Native: `/` e MOD → ERR 11, `\` → 0
' Canone ora unico (runtime C + IL + PCode): divisore 0 → ERR 11 "Division by zero",
' catturabile con TRY/CATCH o ON ERROR; senza handler è un errore fatale (codice d'uscita 6).
' Nota: il valore viene prima assegnato e poi stampato — PRINT con testo + chiamata che fallisce
' lascia testo parziale su IL e lo scarta su PCode/VM-C/C99 (divergenza di print staging, separata).
' EXPECT: slash=caught|back=caught|mod=caught|slash-ok=no-err|back-ok=no-err|mod-ok=no-err|try=caught|13|3|1
FUNCTION DivSlash(a AS DOUBLE, b AS DOUBLE) AS STRING
  ON ERROR GOTO OnBad
  DIM r AS DOUBLE
  r = a / b
  RETURN "no-err"
OnBad:
  RETURN "caught"
END FUNCTION

FUNCTION DivBack(a AS LONG, b AS LONG) AS STRING
  ON ERROR GOTO OnBad2
  DIM r AS LONG
  r = a \ b
  RETURN "no-err"
OnBad2:
  RETURN "caught"
END FUNCTION

FUNCTION DivMod(a AS LONG, b AS LONG) AS STRING
  ON ERROR GOTO OnBad3
  DIM r AS LONG
  r = a MOD b
  RETURN "no-err"
OnBad3:
  RETURN "caught"
END FUNCTION

SUB Main()
  DIM s AS STRING, t AS DOUBLE, k AS LONG

  s = DivSlash(1, 0) : PRINT "slash="; s
  s = DivBack(1, 0) : PRINT "back="; s
  s = DivMod(1, 0) : PRINT "mod="; s

  ' Percorso normale: valori invariati.
  s = DivSlash(6, 3) : PRINT "slash-ok="; s
  s = DivBack(7, 2) : PRINT "back-ok="; s
  s = DivMod(7, 2) : PRINT "mod-ok="; s

  ' TRY/CATCH: l'errore è catturabile anche senza ON ERROR.
  TRY
    t = 1 / 0
    PRINT "try=no-err"
  CATCH e AS ERR
    PRINT "try=caught"
  END TRY

  ' Nessun effetto collaterale sul resto del programma.
  k = 13 : PRINT k
  k = 10 \ 3 : PRINT k
  k = 10 MOD 3 : PRINT k
END SUB
