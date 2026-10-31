' MiniTest: INKEY nudo (gruppo Console, parità 4 backend).
' INKEY var$ [, timeout]: poll non-bloccante della typeahead (SENDKEY inietta).
' Reale sulle 4 gambe; oracolo deterministico via SENDKEY.
' EXPECT: k=Z
IMPORT CONSOLE

SUB Main()
  SENDKEY "Z"
  DIM k AS STRING
  INKEY k
  PRINT "k="; k
END SUB
