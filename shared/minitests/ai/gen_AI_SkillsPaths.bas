' MiniTest: AI.SKILLSPATHS() espone il percorso delle skill del progetto.
' Oracolo PORTATILE: il percorso assoluto dipende dal checkout, quindi si verificano
' il numero di voci (0 = una voce) e la presenza della desinenza `/skills`, non il
' valore assoluto (che cambierebbe su ogni macchina).
' EXPECT: 0|-1
IMPORT AI

SUB Main()
    DIM P() AS STRING
    P() = AI.SKILLSPATHS()
    PRINT UBOUND(P)
    PRINT INSTR(P(0), "/skills") > 0
END SUB
