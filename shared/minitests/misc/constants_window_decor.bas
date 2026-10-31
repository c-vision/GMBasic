' MiniTest: costanti decorazione finestra (%SINGLE/%DOUBLE/%ROUNDED/%CLOSED/%NONE)
'   motore browser (%AUTO/%CHROMIUM/%OBSCURA) e canale (%RECEIVED/%CHCLOSED).
' EXPECT: d0= 0|d1= 1|d2= 2|d3= 3|d4=-1|b0= 0|b1= 1|b2= 2|r1= 1|c0= 0
SUB Main
  PRINT "d0="; %NONE
  PRINT "d1="; %SINGLE
  PRINT "d2="; %DOUBLE
  PRINT "d3="; %ROUNDED
  PRINT "d4="; %CLOSED
  PRINT "b0="; %AUTO
  PRINT "b1="; %CHROMIUM
  PRINT "b2="; %OBSCURA
  PRINT "r1="; %RECEIVED
  PRINT "c0="; %CHCLOSED
END SUB
