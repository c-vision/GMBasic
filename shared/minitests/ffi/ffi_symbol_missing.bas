' EXPECT: e1=5:[FFI] simbolo 'ofx_no_such_symbol_zzz' non trovato in 'gmbgraphics' (libreria non aggiornata?)|ok= 2
' Simbolo FFI non risolto = errore ESPLICITO e CATTURABILE, con lo STESSO testo e lo
' stesso codice (5 = IllegalFunctionCall) su tutte e 4 le gambe.
'
' Prima della regressione coperta qui:
'   IL     : ERR.Number=200 (InternalError) e testo inglese "Function '...' not found in library."
'   PCodeVM: nessun errore — la stringa "[FFI_ERROR] ..." finiva sullo stack come VALORE,
'            quindi il programma proseguiva ("no-err") e il risultato sembrava valido
'   VM/C   : errore FATALE (exit 6): ON ERROR non lo vedeva mai, out() intatti
'   C99    : codice 5 ma testo diverso da VM/C
' Ora tutte le gambe passano da un errore BASIC catturabile con il testo canonico
' (FfiService.FfiSymbolMissingMessage ⇄ gmb_raise_basic_error/gmb_raise_error).
DECLARE FUNCTION MissingSym LIB "gmbgraphics" ALIAS "ofx_no_such_symbol_zzz" () AS LONG

FUNCTION CallMissing() AS STRING
  ON ERROR GOTO OnBad
  DIM v AS LONG
  v = MissingSym()
  RETURN "no-err"
OnBad:
  RETURN LTRIM$(STR$(ERR.Number)) + ":" + ERR.Description
END FUNCTION

SUB Main()
  PRINT "e1=" + CallMissing()
  ' Dopo l'errore catturato il flusso prosegue normalmente.
  PRINT "ok="; 1 + 1
END SUB
