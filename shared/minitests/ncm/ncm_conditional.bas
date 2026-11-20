' NCM.LOADCONDITIONAL / GENERATEFROM con il modello condizionato MINUSCOLO di samples/Fixtures/ncm_tiny_cond (pesi casuali, stessa architettura
' di XSum: prefisso di 4 token, livelli 2/4/8, head legate al codebook, cascata learned_l1, BPE di GPT-2 con due merge). Il testo non ha senso:
' si verificano le proprieta' che devono valere ovunque. same: stesso prompt = stesso testo (greedy); diff: prompt diversi = testi diversi;
' trunc: oltre i primi 4 token il prompt non conta; seedfree: con top_k = 1 il seed non cambia nulla; cfg0: cfgScale 0 = default del config;
' cfgdiff: la guidance (cfgScale 1) cambia il testo; 
' Errori: handle non valido, modello non condizionato (GENERATE su un condizionato, GENERATEFROM su un non condizionato), config inesistente.
' EXPECT: load= 1|nonempty= 1|same= 1|diff= 1|trunc= 1|seedfree= 1|cfg0= 1|cfgdiff= 1|badhandle= 1|uncond= 1|gen_on_cond= 1|badcfg= 0
IMPORT NCM

SUB Main()
    DIM FX AS STRING, UX AS STRING
    FX = "../../samples/Fixtures/ncm_tiny_cond/"
    UX = "../../samples/Fixtures/ncm_tiny/"
    DIM h AS LONG
    h = NCM.LOADCONDITIONAL(FX + "generator.pt", FX + "tokenizer.pt", FX + "config.yaml", FX + "vocab.json", FX + "merges.txt", 0)
    PRINT "load="; IIF(h > 0, 1, 0)

    DIM a AS STRING, b AS STRING, c AS STRING
    a = NCM.GENERATEFROM(h, "the cat sat on the mat", 0, -1)
    PRINT "nonempty="; IIF(LEN(a) > 0, 1, 0)
    b = NCM.GENERATEFROM(h, "the cat sat on the mat", 0, -1)
    PRINT "same="; IIF(a = b, 1, 0)
    c = NCM.GENERATEFROM(h, "hello world", 0, -1)
    PRINT "diff="; IIF(a <> c, 1, 0)
    PRINT "trunc="; IIF(a = NCM.GENERATEFROM(h, "the cat sat on the mat and then a lot more text", 0, -1), 1, 0)
    PRINT "seedfree="; IIF(a = NCM.GENERATEFROM(h, "the cat sat on the mat", 99, -1), 1, 0)
    PRINT "cfg0="; IIF(a = NCM.GENERATEFROM(h, "the cat sat on the mat", 0, 0.0), 1, 0)
    PRINT "cfgdiff="; IIF(a <> NCM.GENERATEFROM(h, "the cat sat on the mat", 0, 1.0), 1, 0)


    NCM.FREEMODEL(h)

    DIM bad AS STRING
    bad = NCM.GENERATEFROM(9999, "x", 0, -1)
    PRINT "badhandle="; IIF(LEN(bad) = 0 AND LEN(NCM.LASTERROR()) > 0, 1, 0)

    DIM u AS LONG
    u = NCM.LOADMODEL(UX + "generator.pt", UX + "tokenizer.pt", UX + "vocab.txt", 0)
    PRINT "uncond="; IIF(LEN(NCM.GENERATEFROM(u, "x", 0, -1)) = 0 AND LEN(NCM.LASTERROR()) > 0, 1, 0)
    NCM.FREEMODEL(u)

    h = NCM.LOADCONDITIONAL(FX + "generator.pt", FX + "tokenizer.pt", FX + "config.yaml", FX + "vocab.json", FX + "merges.txt", 0)
    PRINT "gen_on_cond="; IIF(LEN(NCM.GENERATE(h, 1, 0)) = 0 AND LEN(NCM.LASTERROR()) > 0, 1, 0)
    NCM.FREEMODEL(h)

    PRINT "badcfg="; NCM.LOADCONDITIONAL(FX + "generator.pt", FX + "tokenizer.pt", "/nonexistent/config.yaml", FX + "vocab.json", FX + "merges.txt", 0)
END SUB
