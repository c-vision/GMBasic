' MiniTest SYSTEM.COMMAND / COMMANDCOUNT — argomenti da riga di comando.
' Indice altissimo → stringa vuota su ogni gamba (gli argomenti dell'harness divergono).
' EXPECT: c0=OK|c1=OK|c2=OK|cc=OK|scc=OK
IMPORT SYSTEM
SUB Main()
  IF LEN(COMMAND(999)) = 0 THEN PRINT "c0=OK"
  IF LEN(SYSTEM.COMMAND(999)) = 0 THEN PRINT "c1=OK"
  IF LEN(COMMAND$(999)) = 0 THEN PRINT "c2=OK"
  IF COMMANDCOUNT() >= 0 THEN PRINT "cc=OK"
  IF SYSTEM.COMMANDCOUNT() >= 0 THEN PRINT "scc=OK"
END SUB
