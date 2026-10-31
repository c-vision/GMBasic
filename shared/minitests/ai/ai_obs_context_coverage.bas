' EXPECT: cview=|cviewdbg=|obs={"error":"unknown observation"}|obshash= 0|obscount= 0|obslist=|obsfile=
IMPORT AI

SUB Main()
    PRINT "cview="; AI.CONTEXTVIEW$("test-agent")
    PRINT "cviewdbg="; AI.CONTEXTVIEWDEBUG$("test-agent")
    PRINT "obs="; AI.OBSERVATION$(0)
    PRINT "obshash="; LEN(AI.OBSERVATIONHASH$(0))
    PRINT "obscount="; AI.OBSERVATIONCOUNT()
    PRINT "obslist="; AI.OBSERVATIONLIST$()
    PRINT "obsfile="; AI.OBSERVATIONFILE$(0)
END SUB