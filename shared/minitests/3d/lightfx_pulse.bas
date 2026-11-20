' EXPECT: l1= 1.5|l2= 1|l3= 0.5
' MOTORE 3D — WORLD LIGHTFX modo 2 (pulse): luce = 1 + amount × sin(2π × fase), rate 1 Hz.
' Dopo 0,25 s la fase è 1/4 di periodo → 1 + 0,5 = 1,5; dopo 0,5 s → 1; dopo 0,75 s → 0,5.
' Il valore vive in WORLD.LIGHTNOW, che è l'unica via per leggerlo (§1: LIGHTFX è comando).
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD LIGHTFX 0, 2, 0.5, 1
  WORLD TICK 0.25
  PRINT "l1="; INT(WORLD.LIGHTNOW(0) * 100 + 0.5) / 100
  WORLD TICK 0.25
  PRINT "l2="; INT(WORLD.LIGHTNOW(0) * 100 + 0.5) / 100
  WORLD TICK 0.25
  PRINT "l3="; INT(WORLD.LIGHTNOW(0) * 100 + 0.5) / 100
END SUB
