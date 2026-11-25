' MiniTest: DATETIME in forma NUDO-VALORE (senza parentesi) — TIME, DATE, NOW, TODAY.
' Il C99 le emetteva come variabili implicite 0 (sample Http/HttpServer: `info("time") = TIME`
' dava 0); IL/PCode/VM-C le valutano come la call a 0 argomenti. Asserzioni STABILI sul
' VALORE (seriale OADate): > 40000 = dopo il 2009. (Il formato di stampa è coperto altrove;
' `LEN(NOW)` non è usato: su VM/C dà 0 e non è il canone di questo test.)
' EXPECT: t=-1|d=-1|n=-1|td=-1
SUB Main()
  PRINT "t="; (TIME > 40000)
  PRINT "d="; (DATE > 40000)
  PRINT "n="; (NOW > 40000)
  PRINT "td="; (TODAY > 40000)
END SUB
