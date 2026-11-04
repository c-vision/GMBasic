' MiniTest SPEED — VALIDATE nelle 4 arità registrate (parità 4 backend).
'
' Firme: VALIDATE(), VALIDATE(alias$), VALIDATE(full), VALIDATE(alias$, full).
' Semantica: integrità del DB SPEED corrente; Bit canonico -1 valido / 0 non valido.
' Prima: C99Native non emetteva il simbolo ("funzione non definita"), IL andava in
' ILGEN "conversione numerica da Double a String" sulle varianti con Bit, PCode
' restituiva 1.0 (non -1) e non risolveva l'alias del database ("V").
'
' EXPECT: pre= 0|v0=-1|v1=-1|v2=-1|v3=-1
IMPORT SPEED

TYPE Riga
  ID AS LONG
  Nome AS STRING * 16
END TYPE

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR RESUME NEXT
  RMDIR GMBWORK$ + "/validate_db", TRUE
  ON ERROR GOTO 0
  MKDIR GMBWORK$
  PRINT "pre="; SPEED.VALIDATE()
  SPEED OPEN GMBWORK$ + "/validate_db" ALIAS V
  SPEED USE "T" FROM V OF Riga ALIAS T INDEXED BY ID
  PRINT "v0="; SPEED.VALIDATE()
  PRINT "v1="; SPEED.VALIDATE("V")
  PRINT "v2="; SPEED.VALIDATE(-1)
  PRINT "v3="; SPEED.VALIDATE("V", -1)
  SPEED CLOSE DATABASES
END SUB
