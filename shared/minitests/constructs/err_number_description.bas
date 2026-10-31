' MiniTest — ERR.Number / ERR.Description popolati su TUTTE e 4 le gambe.
'
' Divergenza trovata chiudendo la divisione per zero: `ERR.Number`/`ERR.Description` erano
' inutilizzabili fuori da IL/C99.
'   IL     : OK, ma in un handler ON ERROR il codice era SEMPRE 200 (BasicException ri-avvolta
'            dal catch generico del chunk: FromClrException -> InternalError)
'   PCodeVM: ERR mai popolata (dizionario globale assente) -> n=0, d=""
'   VM/C   : idem (il raise a 3 livelli non scriveva il globale ERR)
'   C99    : OK per la divisione; `ERROR n` fuori TRY stampava e usciva dalla procedura
'            (non catturabile da ON ERROR GOTO)
' Ora tutte le gambe riportano codice, descrizione e RIGA ESATTA (1-based, quella dello
' statement che ha fallito) sia in `ON ERROR GOTO` sia in `CATCH e AS ERR`; `ERROR n` senza
' testo usa il catalogo canonico ("Division by zero").
' EXPECT: slash=E11:Division by zero@L18|back=E11:Division by zero@L27|mod=E11:Division by zero@L36|expl=E11:Division by zero@L52|try=E11:Division by zero@L59|throw=E42:Unspecified runtime error@L67|ok=no-err
FUNCTION DivSlash(a AS DOUBLE, b AS DOUBLE) AS STRING
  ON ERROR GOTO OnBad
  DIM r AS DOUBLE
  r = a / b
  RETURN "no-err"
OnBad:
  RETURN "E" + LTRIM$(STR$(ERR.Number)) + ":" + ERR.Description + "@L" + LTRIM$(STR$(ERR.LineNumber))
END FUNCTION

FUNCTION DivBack(a AS LONG, b AS LONG) AS STRING
  ON ERROR GOTO OnBad2
  DIM r AS LONG
  r = a \ b
  RETURN "no-err"
OnBad2:
  RETURN "E" + LTRIM$(STR$(ERR.Number)) + ":" + ERR.Description + "@L" + LTRIM$(STR$(ERR.LineNumber))
END FUNCTION

FUNCTION DivMod(a AS LONG, b AS LONG) AS STRING
  ON ERROR GOTO OnBad3
  DIM r AS LONG
  r = a MOD b
  RETURN "no-err"
OnBad3:
  RETURN "E" + LTRIM$(STR$(ERR.Number)) + ":" + ERR.Description + "@L" + LTRIM$(STR$(ERR.LineNumber))
END FUNCTION

SUB Main()
  DIM s AS STRING, x AS DOUBLE

  ' ON ERROR dentro una FUNCTION: ERR.Number/Description letti nell'handler.
  s = DivSlash(1, 0) : PRINT "slash=" + s
  s = DivBack(1, 0) : PRINT "back=" + s
  s = DivMod(1, 0) : PRINT "mod=" + s

  ' ERROR esplicito SENZA descrizione: catalogo canonico del codice (non stringa vuota).
  ON ERROR GOTO HExpl
  ERROR 11
HExpl:
  PRINT "expl=E" + LTRIM$(STR$(ERR.Number)) + ":" + ERR.Description + "@L" + LTRIM$(STR$(ERR.LineNumber))
  ON ERROR GOTO 0

  ' TRY/CATCH: la variabile CATCH AS ERR porta gli stessi campi.
  TRY
    x = 1 / 0
    PRINT "try=no-err"
  CATCH e AS ERR
    PRINT "try=E" + LTRIM$(STR$(e.Number)) + ":" + e.Description + "@L" + LTRIM$(STR$(e.LineNumber))
  END TRY

  ' THROW di un codice numerico: è un errore BASIC come `ERROR n` (non un'uscita muta).
  ON ERROR GOTO HThrow
  THROW 42
HThrow:
  PRINT "throw=E" + LTRIM$(STR$(ERR.Number)) + ":" + ERR.Description + "@L" + LTRIM$(STR$(ERR.LineNumber))
  ON ERROR GOTO 0

  ' Percorso normale invariato.
  s = DivSlash(6, 3) : PRINT "ok="; s
END SUB
