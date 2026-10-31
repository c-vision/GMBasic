' MiniTest: campo UDT annidato a 2 LIVELLI (stazione.pos.lat) via campo di classe —
' scrittura nuda e SELF, lettura nuda/SELF via metodo, e lettura ESTERNA a 1 livello
' (c.stazione.codice). Bug (precedente): IL non risolveva il campo annidato su FieldTarget.
' EXPECT: lat= 45.07|lon= 9.18|code=AB
TYPE Coord
  lat AS DOUBLE
  lon AS DOUBLE
END TYPE
TYPE Stazione
  pos AS Coord
  codice AS STRING * 6
END TYPE
CLASS C
  PUBLIC DIM stazione AS Stazione
  PUBLIC SUB NEW()
    stazione.pos.lat = 45.07            ' catena a 2 livelli (nudo)
    SELF.stazione.pos.lon = 9.18        ' catena a 2 livelli (qualificato)
    stazione.codice = "AB"
  END SUB
  FUNCTION Lat() AS DOUBLE
    Lat = stazione.pos.lat
  END FUNCTION
  FUNCTION Lon() AS DOUBLE
    Lon = SELF.stazione.pos.lon
  END FUNCTION
END CLASS
SUB Main()
  DIM c AS C = NEW C()
  PRINT "lat="; c.Lat()
  PRINT "lon="; c.Lon()
  PRINT "code="; c.stazione.codice        ' lettura esterna (1 livello di annidamento)
END SUB
