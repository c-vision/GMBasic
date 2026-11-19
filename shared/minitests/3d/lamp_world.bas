' EXPECT: t= 30|l= 1|m= 0|c= 64
' TAG: native-only
' MOTORE 3D — WORLD LAMP aggiunge una lampada puntiforme (W-LIGHT: schiarisce e basta).
' La lampada NON cambia la luce del settore (WORLD.LIGHTNOW resta 1: quella è la luce
' ambientale/locale del settore, LIGHTFX la modula) e non tocca quote né mover. Il demo ha già
' una lampada nel documento: questa si somma, quindi il limite di 8 per mondo non scatta.
' L'effetto visivo è coperto dalle golden del motore, non da questo MiniTest.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD LAMP 40, 40, 50, 90, 0.6
  WORLD TICK 0.5
  PRINT "t="; INT(WORLD.TIME() * 60 + 0.5)
  PRINT "l="; INT(WORLD.LIGHTNOW(0) * 100 + 0.5) / 100
  PRINT "m="; WORLD.MOVING(0)
  PRINT "c="; INT(WORLD.CEILZ(4) * 100 + 0.5) / 100
END SUB
