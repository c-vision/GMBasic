' EXPECT: t= 30|f= 0|c= 64|m= 0
' TAG: native-only
' MOTORE 3D — WORLD SCROLL what 0 = piani, what 1 = pareti del settore.
' Gli offset integrati non sono leggibili dal BASIC: qui si verifica che il comando sia
' accettato su entrambi i bersagli e che non sposti quote né avvii mover; l'aspetto visivo
' resta alle golden del motore. Il TICK dopo lo scroll prova che l'integrazione non ferma
' l'orologio né il passo fisso.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SCROLL 0, 0, 8, 0
  WORLD SCROLL 0, 1, 0, 4
  WORLD TICK 0.5
  PRINT "t="; INT(WORLD.TIME() * 60 + 0.5)
  PRINT "f="; INT(WORLD.FLOORZ(0) * 100 + 0.5) / 100
  PRINT "c="; INT(WORLD.CEILZ(0) * 100 + 0.5) / 100
  PRINT "m="; WORLD.MOVING(0)
END SUB
