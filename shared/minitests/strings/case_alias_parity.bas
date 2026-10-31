' UCASE / LCASE sono ALIAS di UPPER / LOWER (decisione utente 2026-10-27): stessa procedura,
' stesso TokenKind canonico (Upper/Lower), nessun token "2". Qui si verifica che:
'   - le due grafie producano lo STESSO risultato delle forme canoniche;
'   - l'argomento sia una sola stringa (parametro unico, in entrambe le forme);
'   - i due nomi risolvano anche qualificati (STRINGS.UCASE / STRINGS.LCASE).
' EXPECT: ucase=ABC|lcase=abc|upper=ABC|lower=abc|ucase=upper|lcase=lower|str=si|qual=ABC
IMPORT STRINGS
SUB Main()
  PRINT "ucase="; UCASE("aBc")
  PRINT "lcase="; LCASE("AbC")
  PRINT "upper="; UPPER("aBc")
  PRINT "lower="; LOWER("AbC")

  ' stesso risultato delle forme canoniche (alias, non funzione distinta)
  IF UCASE("x") = UPPER("x") THEN PRINT "ucase=upper" ELSE PRINT "ucase=upper-FAIL"
  IF LCASE("Y") = LOWER("Y") THEN PRINT "lcase=lower" ELSE PRINT "lcase=lower-FAIL"

  ' parametro: stringa non ASCII-safe → resta una sola stringa, nessuna troncatura
  IF LEN(UCASE("citta")) = 5 THEN PRINT "str=si" ELSE PRINT "str=no"

  ' forma qualificata sul namespace STRINGS
  PRINT "qual="; STRINGS.UCASE("abc")
END SUB
