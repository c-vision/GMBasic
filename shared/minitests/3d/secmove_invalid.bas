' EXPECT: f1= 0|m1= 0|f2= 0|m2= 0
' TAG: native-only
' MOTORE 3D — WORLD SECMOVE rifiuta gli argomenti non validi SENZA muovere niente.
' Il comando non ha un valore di ritorno leggibile dal BASIC (§1: i comandi sono statement),
' quindi la prova è lo STATO: con il soffitto sotto il pavimento, o con un settore inesistente,
' il pavimento resta a 0 e nessun mover parte (WORLD.MOVING resta 0).
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 100, 50, 40
  PRINT "f1="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "m1="; WORLD.MOVING(1)
  WORLD SECMOVE 99, 56, 28
  PRINT "f2="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "m2="; WORLD.MOVING(1)
END SUB
