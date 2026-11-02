' Golden sample: controllo di flusso + stringhe (AST/disasm/C stabili).
SUB Main()
  DIM i AS INTEGER
  DIM s AS STRING
  s = ""
  FOR i = 1 TO 3
    IF i MOD 2 = 1 THEN
      s = s + REPLACE$("a-b", "-", "+") + "|"
    ELSE
      s = s + "even|"
    END IF
  NEXT
  PRINT s
END SUB
