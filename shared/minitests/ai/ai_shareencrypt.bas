' MiniTest: AI.ShareEncrypt roundtrip (ShareEncrypt e' randomizzato per-run: IV diverso
' a ogni chiamata, quindi nessun oracolo sul ciphertext; l'oracolo e' sul roundtrip).
' Copre ShareEncrypt (era mancante) + ShareDecrypt.
' EXPECT: roundtrip=hello
IMPORT AI WITH ALIAS
SUB Main()
  DIM c$ = AI.ShareEncrypt("hello", "key123")
  PRINT "roundtrip="; AI.ShareDecrypt(c$, "key123")
END SUB
