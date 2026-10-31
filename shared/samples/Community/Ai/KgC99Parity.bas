' FILE: Ai/KgC99Parity.bas — verifica fase C: AI.KG* su transpiler C99 (emissione gmb_kg_*)
' puramente C, nessun LLM. Esito: stampa OK.
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "KG="; AI.KGADDNODE("alice")
    PRINT AI.KGADDNODE("bob")
    PRINT AI.KGADDNODE("carol")
    PRINT "EDGE="; AI.KGADDEDGE("alice", "bob")
    PRINT AI.KGADDEDGE("bob", "carol")
    PRINT "Q="; AI.KGQUERY("alice")
    PRINT "P="; AI.KGQUERY("path alice->carol")
    PRINT "OK"
END SUB
