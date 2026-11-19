' EXPECT: s1= 1|s2= 0.5|off= 1
' TAG: native-only
' MOTORE 3D — WORLD LIGHTFX modo 3 (strobe a mezzo periodo) e modo 0 (effetto spento).
' Con rate 1 Hz e amount 0,5: nella prima metà del periodo la luce è piena (1), nella seconda
' è 1 - amount (0,5). Il modo 0 azzera l'effetto e riporta la luce a 1 subito, senza aspettare
' un TICK: è il comando che "spegne il flicker" quando la scena cambia.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD LIGHTFX 0, 3, 0.5, 1
  WORLD TICK 0.25
  PRINT "s1="; INT(WORLD.LIGHTNOW(0) * 100 + 0.5) / 100
  WORLD TICK 0.5
  PRINT "s2="; INT(WORLD.LIGHTNOW(0) * 100 + 0.5) / 100
  WORLD LIGHTFX 0, 0, 0, 0
  PRINT "off="; INT(WORLD.LIGHTNOW(0) * 100 + 0.5) / 100
END SUB
