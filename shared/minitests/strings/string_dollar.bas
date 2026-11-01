' MiniTest STRINGS — `STRING$(n, ch)` parità 4 gambe.
'
' La riscrittura del linker (STRING$ → REPEAT) trattava il secondo argomento come stringa:
'   - con una STRINGA il C99 non compilava (gmb_chr(gmb_string_t) — bug chiuso);
'   - con un CODICE numerico la gamba IL produceva IL INVALIDO (InvalidProgramException) e
'     le altre davano "" o la rappresentazione testuale del numero.
' Ora: STRINGA → n copie del PRIMO carattere (`"ab"` × 3 = `"aaa"`, semantica DUP);
' NUMERO → n copie di CHR(codice) (semantica classica BASIC).
'
' EXPECT: a=[aaa]|b=[aaa]|c=[AAA]|d=[]|e=[]|f=[aaa]
IMPORT STRINGS

SUB Main()
  ' (a) stringa di un carattere
  PRINT "a=["; STRING$(3, "a"); "]"
  ' (b) stringa multi-carattere: si usa il PRIMO carattere (semantica DUP, classica BASIC)
  PRINT "b=["; STRING$(3, "ab"); "]"
  ' (c) codice numerico: n copie di CHR(codice)
  PRINT "c=["; STRING$(3, 65); "]"
  ' (d) zero copie
  PRINT "d=["; STRING$(0, "z"); "]"
  ' (e) conteggio negativo → stringa vuota (nessun errore)
  PRINT "e=["; STRING$(-2, "z"); "]"
  ' (f) codice in una VARIABILE numerica (stessa semantica del letterale)
  DIM code AS INTEGER
  code = 97
  PRINT "f=["; STRING$(3, code); "]"
END SUB
