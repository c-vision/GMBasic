' EXPECT: m1=-1|m2=-1
' TAG: native-only
' MOTORE 3D — WORLD HEIGHT su un settore inesistente è un errore esplicito e non tocca il
' mondo: il mover del settore 1 continua a correre. È il controllo che il comando non usi
' un indice fuori range come "ferma tutto".
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 56, 28
  WORLD TICK 0.1
  PRINT "m1="; WORLD.MOVING(1)
  WORLD HEIGHT 99
  PRINT "m2="; WORLD.MOVING(1)
END SUB
