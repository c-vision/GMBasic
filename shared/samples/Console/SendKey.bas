' FILE: samples/Console/SendKey.bas
' SENDKEY — test typeahead per programmi interattivi
' Invia testo simulato al buffer di input della console, poi lo legge
SUB Main()
  DIM s$
  SENDKEY "Ciao Mondo" + CHR(13)
  SENDKEY "Hello World" + CHR(13)
  PRINT "SENDKEY inviato"
END SUB