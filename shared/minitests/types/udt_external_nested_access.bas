' MiniTest: accesso ESTERNO a campi UDT annidati su un'ISTANZA —
'   (1) catena UDT profonda `c.stazione.pos.lat` (>= 3 livelli);
'   (2) campo di un elemento di array UDT 2-D `c.griglia(1,1).valore` / `.tag` (anche STRING * N).
' EXPECT: lat= 45.07|val= 9|tag=T1
TYPE Coord
  lat AS DOUBLE
END TYPE
TYPE Stazione
  pos AS Coord
END TYPE
TYPE Cella
  valore AS INTEGER
  tag AS STRING * 4
END TYPE
CLASS C
  PUBLIC DIM stazione AS Stazione
  PUBLIC DIM griglia(1, 1) AS Cella
  PUBLIC SUB NEW()
    stazione.pos.lat = 45.07
    griglia(1, 1).valore = 9
    griglia(1, 1).tag = "T1"
  END SUB
END CLASS
SUB Main()
  DIM c AS C = NEW C()
  PRINT "lat="; c.stazione.pos.lat        ' catena esterna a 3 livelli
  PRINT "val="; c.griglia(1, 1).valore    ' campo di elemento UDT 2-D esterno
  PRINT "tag="; c.griglia(1, 1).tag       ' STRING * 4 su elemento UDT 2-D esterno
END SUB
