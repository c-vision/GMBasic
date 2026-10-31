' FILE: Classes/ClassesNestedUdt.bas
' =================================================================================================
'   GUIDA: CAMPI UDT ANNIDATI + STRING a LUNGHEZZA FISSA (STRING * N) + ARRAY 2-D
' =================================================================================================
'
' Un campo di una CLASS puo' essere a sua volta un UDT (TYPE), anche annidato a piu' livelli:
' si accede con la notazione a punto (r.campo, SELF.r.campo, a.b.c), nudo o qualificato.
' Un campo di un TYPE NON puo' essere una STRING dinamica: usa STRING * N (lunghezza fissa,
' paddata con spazi in scrittura; LEN = N in lettura).
'
' Copre, con parita' 4 backend:
'   - campo UDT annidato su 2 livelli (stazione.pos.lat), nudo e SELF;
'   - STRING * N (codice a 6 caratteri): padding + LEN;
'   - accesso a campo di elemento UDT 2-D (griglia(1,1).valore / .tag);
'   - array-membro 1-D (letture(3)) e 2-D (griglia(1,1));
'   - lettura esterna di un campo UDT annidato a 1 livello (s.stazione.codice).

TYPE Coord
  lat AS DOUBLE
  lon AS DOUBLE
END TYPE

TYPE Stazione
  codice AS STRING * 6        ' sempre 6 caratteri (riempito con spazi)
  pos AS Coord                ' UDT annidato (stazione.pos.lat)
END TYPE

TYPE Cella
  valore AS INTEGER
  tag AS STRING * 4
END TYPE

CLASS Sensore
  PUBLIC DIM stazione AS Stazione
  PUBLIC DIM griglia(1, 1) AS Cella
  PUBLIC DIM letture(3) AS DOUBLE

  PUBLIC SUB NEW()
    stazione.codice = "AB"              ' accesso nudo, campo STRING * 6
    stazione.pos.lat = 45.07            ' catena a 2 livelli (nudo)
    SELF.stazione.pos.lon = 9.18        ' catena a 2 livelli (qualificato)
    griglia(1, 1).valore = 9            ' campo di un elemento UDT 2-D
    griglia(1, 1).tag = "T1"            ' STRING * 4 su elemento 2-D
  END SUB

  SUB SetLettura(i AS INTEGER, v AS DOUBLE)
    letture(i) = v
  END SUB

  FUNCTION Codice() AS STRING
    Codice = stazione.codice
  END FUNCTION

  FUNCTION CodiceLen() AS INTEGER
    CodiceLen = LEN(stazione.codice)    ' STRING * 6 -> 6
  END FUNCTION

  FUNCTION Lat() AS DOUBLE
    Lat = stazione.pos.lat              ' catena a 2 livelli (nudo)
  END FUNCTION

  FUNCTION Lon() AS DOUBLE
    Lon = SELF.stazione.pos.lon         ' catena a 2 livelli (qualificato)
  END FUNCTION

  FUNCTION GridValue() AS INTEGER
    GridValue = griglia(1, 1).valore    ' campo di elemento UDT 2-D
  END FUNCTION

  FUNCTION GridTag() AS STRING
    GridTag = griglia(1, 1).tag
  END FUNCTION
END CLASS

SUB Main()
  DIM s AS Sensore = NEW Sensore()
  s.SetLettura(0, 1.5)
  s.SetLettura(1, 2.5)

  PRINT "cod="; s.stazione.codice        ' lettura esterna di campo UDT annidato (1 livello)
  PRINT "len="; s.CodiceLen()             ' STRING * 6 -> 6
  PRINT "lat="; s.stazione.pos.lat        ' catena ESTERNA a 2 livelli (istanza.a.b.c)
  PRINT "lon="; s.stazione.pos.lon
  PRINT "grid="; s.griglia(1, 1).valore   ' campo di elemento UDT 2-D ESTERNO
  PRINT "tag="; s.griglia(1, 1).tag       ' STRING * 4 su elemento 2-D esterno
  PRINT "latM="; s.Lat()                  ' stesso valore via metodo (dentro la classe)
  PRINT "gridM="; s.GridValue()
  PRINT "sum="; s.letture(0) + s.letture(1)
END SUB
