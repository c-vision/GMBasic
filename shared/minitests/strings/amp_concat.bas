' MiniTest: concatenazione di stringhe con '&' (idioma BASIC) — parità 4 backend.
' REGRESSIONE: prima l'operatore '&' binario era IGNORATO in silenzio (restava il solo
' operando sinistro: `PRINT "a" & x` stampava "a") e in un argomento di chiamata seguito
' da un comando LLM dava "Expected ')' after arguments". Ora '&' è concat (path di '+').
' EXPECT: ab|aX|Xb|pqX|via|via
SUB Main()
  DIM x AS STRING : x = "X"
  DIM y AS STRING
  y = "a" & "b" : PRINT y
  y = "a" & x   : PRINT y
  y = x & "b"   : PRINT y
  PRINT "p" & "q" & x
  PRINT "vi" & "a"
  PRINT "v" & "ia"
END SUB
