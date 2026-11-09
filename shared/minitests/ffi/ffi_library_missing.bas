' EXPECT: e=53:[FFI] libreria 'no_such_lib_zzz' non trovata|ok= 2
' DECLARE ... LIB verso una libreria che NON esiste: errore ESPLICITO e CATTURABILE,
' codice 53 (FileNotFound) e stesso testo sulle 4 gambe.
'
' Prima della regressione coperta qui:
'   IL     : 53 ma testo inglese "Library not found: no_such_lib_zzz"
'   PCodeVM: 53 ma stesso testo inglese
'   VM/C   : 5 con "[FFI] libreria non trovata: X (simbolo Y)"
'   C99    : la lib assente degradava nel messaggio del SIMBOLO ("simbolo 'X' non trovato
'            in 'Y' (libreria non aggiornata?)"): mandava a cercare il simbolo invece della
'            libreria, col codice 5 invece di 53
' Ora tutte le gambe sollevano 53 col testo canonico
' (FfiService.FfiLibraryMissingMessage ⇄ gmb_raise_error(53, ...) / gmb_raise_basic_error).
DECLARE FUNCTION NoLib LIB "no_such_lib_zzz" ALIAS "anything" () AS LONG

FUNCTION CallNoLib() AS STRING
  ON ERROR GOTO OnBad
  DIM v AS LONG
  v = NoLib()
  RETURN "no-err"
OnBad:
  RETURN LTRIM$(STR$(ERR.Number)) + ":" + ERR.Description
END FUNCTION

SUB Main()
  PRINT "e=" + CallNoLib()
  ' Dopo l'errore catturato il flusso prosegue normalmente.
  PRINT "ok="; 1 + 1
END SUB
