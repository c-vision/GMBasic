' EXPECT: m3=-1|m0= 0|m2= 0|z2= 0
' TAG: native-only
' MOTORE 3D — WORLD.MOVING è PER SETTORE: avviare il 3 non muove gli altri.
' Ascensori indipendenti sono questo: SECMOVE 3 lascia fermi 0 e 2, e il pavimento del 2
' resta a quota 0 anche dopo un secondo di simulazione.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 3, 20, 4
  PRINT "m3="; WORLD.MOVING(3)
  PRINT "m0="; WORLD.MOVING(0)
  PRINT "m2="; WORLD.MOVING(2)
  WORLD TICK 1
  PRINT "z2="; INT(WORLD.FLOORZ(2) * 100 + 0.5) / 100
END SUB
