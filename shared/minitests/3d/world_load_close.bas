' EXPECT: t0= 0|t1= 30|t2= 0
' TAG: native-only
' MOTORE 3D — dominio WORLD: load, tick, close.
' Il BASIC non maneggia handle: c'è UN mondo corrente (come DBF/ISAM). Dopo WORLD CLOSE non
' resta nessun mondo, quindi WORLD.TIME() torna 0: è l'unica prova osservabile che il close
' è avvenuto davvero (nessuna funzione espone l'handle).
' Il tempo si legge in TICK (1/60 s, passo fisso T-FIXED) per non dipendere dalla stampa
' dei double: t0/t1/t2 sono numeri interi esatti.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  PRINT "t0="; INT(WORLD.TIME() * 60)
  WORLD TICK 0.5
  PRINT "t1="; INT(WORLD.TIME() * 60 + 0.5)
  WORLD CLOSE
  PRINT "t2="; INT(WORLD.TIME() * 60)
END SUB
