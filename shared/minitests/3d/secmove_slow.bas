' EXPECT: z1= 2.5|m=-1|z2= 5|m2= 0
' TAG: native-only
' MOTORE 3D — WORLD SECMOVE con velocità bassa: 10 unità/s per 0,25 s sono 2,5.
' Serve mezzo secondo per arrivare a 5: il test fissa il legame velocità/tempo (non un
' "arrivo immediato") e il passaggio in moto → fermo alla quota esatta.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 10, 5
  WORLD TICK 0.25
  PRINT "z1="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "m="; WORLD.MOVING(1)
  WORLD TICK 0.25
  PRINT "z2="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "m2="; WORLD.MOVING(1)
END SUB
