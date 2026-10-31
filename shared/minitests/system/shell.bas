' EXPECT: sh-ok|sc-ok|cc-ok|cmd-ok
IMPORT SYSTEM
SUB Main()
  SET SHELL ON
  DIM out$ = SYSTEM.SHELLOUT("echo hi")
  IF LEN(out$) >= 0 THEN PRINT "sh-ok"
  IF SYSTEM.SHELLCODE("true") = 0 THEN PRINT "sc-ok"
  IF SYSTEM.COMMANDCOUNT() >= 0 THEN PRINT "cc-ok"
  DIM c$ = SYSTEM.COMMAND(0)
  IF LEN(c$) >= 0 THEN PRINT "cmd-ok"
END SUB
