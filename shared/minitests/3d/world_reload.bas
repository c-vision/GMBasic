' EXPECT: t1= 60|t2= 0
' MOTORE 3D — dominio WORLD: un secondo LOAD sostituisce il mondo.
' Il load è ATOMICO (R11): il mondo nuovo prende il posto del precedente solo se il documento
' è valido, e il vecchio viene liberato. Il tempo riparte da zero: è la prova che il mondo
' corrente è quello nuovo.
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD TICK 1
  PRINT "t1="; INT(WORLD.TIME() * 60 + 0.5)
  WORLD LOAD "worlds/demo.gmbscene"
  PRINT "t2="; INT(WORLD.TIME() * 60)
END SUB
