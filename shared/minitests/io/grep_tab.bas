' EXPECT: n= 0|tab=-1
' GREP.MATCHES su file con TAB: il testo dell'hit deve sopravvivere al JSON
' (prima il serializer C non escapava \t -> JSON invalido -> IL scartava tutto).
IMPORT GREP
SUB Main()
  DIM h$() AS STRING
  h$ = GREP.MATCHES("Alpha", "../../samples/Fixtures/grep_tab.txt")
  PRINT "n="; UBOUND(h$)
  IF UBOUND(h$) >= 0 THEN
    PRINT "tab="; (INSTR(h$(0), CHR$(9)) > 0)
  ELSE
    PRINT "tab=0"
  END IF
END SUB
