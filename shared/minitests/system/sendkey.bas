' EXPECT: sendkey-ok
' SENDKEY — invia testo al buffer typeahead (parità 4 backend)
SUB Main()
  SENDKEY "test input" + CHR(13)
  PRINT "sendkey-ok"
END SUB