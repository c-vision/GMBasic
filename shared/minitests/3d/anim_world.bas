' EXPECT: t= 30|f= 0|c= 64|m= 0
' TAG: native-only
' MOTORE 3D — WORLD ANIM cicla le picture del settore e NON tocca il resto del mondo.
' Le picture non sono leggibili dal BASIC (nessuna funzione le espone): qui si verifica che
' l'animazione non sposti quote, non avvii mover e non fermi l'orologio. L'aspetto visivo è
' coperto dalle golden del motore (shared/minitests/3d/golden), non da questo MiniTest.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD ANIM 0, 6, 2, 4
  WORLD TICK 0.5
  PRINT "t="; INT(WORLD.TIME() * 60 + 0.5)
  PRINT "f="; INT(WORLD.FLOORZ(0) * 100 + 0.5) / 100
  PRINT "c="; INT(WORLD.CEILZ(0) * 100 + 0.5) / 100
  PRINT "m="; WORLD.MOVING(0)
END SUB
