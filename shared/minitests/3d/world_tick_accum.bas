' EXPECT: t1= 0|t2= 1|t3= 3
' MOTORE 3D — WORLD TICK accoda i secondi e avanza a passi FISSI di 1/60 (T-FIXED).
' Tre TICK da 5 ms valgono 15 ms: NON bastano a un passo (16,6 ms), quindi il mondo non
' avanza. Due ms dopo la coda arriva a 17 ms: parte UN passo. Quaranta ms dopo il residuo
' bastano a due passi (3 in totale). Il test distingue "passo fisso con accumulo" da
' "avanza della durata chiesta".
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD TICK 0.005
  WORLD TICK 0.005
  WORLD TICK 0.005
  PRINT "t1="; INT(WORLD.TIME() * 60 + 0.5)
  WORLD TICK 0.002
  PRINT "t2="; INT(WORLD.TIME() * 60 + 0.5)
  WORLD TICK 0.04
  PRINT "t3="; INT(WORLD.TIME() * 60 + 0.5)
END SUB
