' EXPECT: enum-1= 1|enum-2= 2|enum-10= 10|enum-11= 11
PUBLIC ENUM StatoOrdine AS INTEGER
    Nuovo = 1
    InLavorazione
    Spedito = 10
    Consegnato
END ENUM
SUB Main()
    PRINT "enum-1="; StatoOrdine.Nuovo
    PRINT "enum-2="; StatoOrdine.InLavorazione
    PRINT "enum-10="; StatoOrdine.Spedito
    PRINT "enum-11="; StatoOrdine.Consegnato
END SUB
