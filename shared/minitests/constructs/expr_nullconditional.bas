' EXPECT: nil-nome=|nil-eta=|ok-nome=Mario|ok-eta= 30
TYPE Contatto
    Nome AS STRING * 32
    Eta  AS INTEGER
END TYPE
SUB Main()
    DIM c AS Contatto?
    c = NIL
    PRINT "nil-nome="; c?.Nome
    PRINT "nil-eta="; c?.Eta
    c = NEW Contatto()
    c.Nome = "Mario" : c.Eta = 30
    PRINT "ok-nome="; c?.Nome
    PRINT "ok-eta="; c?.Eta
END SUB
