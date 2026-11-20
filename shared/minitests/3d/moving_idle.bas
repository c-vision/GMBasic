' EXPECT: m0= 0|m3= 0|m4= 0
' MOTORE 3D — WORLD.MOVING vale 0 quando il settore è fermo (e -1 in moto).
' Un mondo appena caricato non ha nessun mover attivo: tutti i settori sono fermi. Il test
' fissa il significato del valore e la lettura per un settore qualsiasi (0, 3, 4 del demo).
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  PRINT "m0="; WORLD.MOVING(0)
  PRINT "m3="; WORLD.MOVING(3)
  PRINT "m4="; WORLD.MOVING(4)
END SUB
