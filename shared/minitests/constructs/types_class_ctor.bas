' EXPECT: ctor|aperte= 1|fine
PUBLIC CLASS Risorsa
    PUBLIC DIM Nome AS STRING
    PUBLIC STATIC DIM Aperte AS LONG = 0
    PUBLIC SUB NEW(nome AS STRING)
        SELF.Nome = nome
        Risorsa.Aperte = Risorsa.Aperte + 1
    END SUB
    PUBLIC SUB DESTRUCTOR
        Risorsa.Aperte = Risorsa.Aperte - 1
    END SUB
END CLASS
SUB Main()
    PRINT "ctor"
    DIM r = NEW Risorsa("db")
    PRINT "aperte="; Risorsa.Aperte
    PRINT "fine"
END SUB
