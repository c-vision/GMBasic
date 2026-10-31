' EXPECT: Error: handle team sconosciuto 'x' (TeamRunStart non chiamato o già consumato)
IMPORT AI

SUB Main()
    PRINT AI.TEAMRUNWAIT("x")
END SUB
