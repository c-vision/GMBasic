' NCM.GENERATE (FFI su libgmb_torch) con il modello NCM MINUSCOLO di samples/Fixtures/ncm_tiny (pesi casuali, 4 livelli
' da 1 a 8 posizioni, vocabolario da 128 voci): il testo non ha senso, ma forma, riproducibilita' e vocabolario si verificano.
' Il test non confronta testi esatti (dipenderebbero dall'aritmetica in virgola mobile della macchina) ma proprieta'
' che devono valere ovunque: n righe, righe non vuote, stesso seed = stesso testo, seed diversi = testi diversi,
' n = 1, seed negativo (casuale) e parole del vocabolario (w<numero> o pezzi ##).
' EXPECT: load= 1|lines= 4|nonempty= 1|same= 1|diff= 1|one= 1|neg= 1|vocab= 1
IMPORT NCM

SUB Main()
    DIM FX AS STRING
    FX = "../../samples/Fixtures/ncm_tiny/"
    DIM h AS LONG
    h = NCM.LOADMODEL(FX + "generator.pt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    PRINT "load="; IIF(h > 0, 1, 0)

    DIM a AS STRING, b AS STRING, c AS STRING
    a = NCM.GENERATE(h, 4, 7)
    DIM rows() AS STRING
    rows = SPLIT(a, CHR$(10))
    ' il testo termina con newline: l'ultimo elemento dopo lo split e' vuoto
    PRINT "lines="; UBOUND(rows)

    DIM ok AS LONG
    ok = 1
    DIM i AS LONG
    FOR i = 0 TO UBOUND(rows) - 1
        IF LEN(rows(i)) = 0 THEN ok = 0
    NEXT i
    PRINT "nonempty="; ok

    b = NCM.GENERATE(h, 4, 7)
    PRINT "same="; IIF(a = b, 1, 0)
    c = NCM.GENERATE(h, 4, 8)
    PRINT "diff="; IIF(a <> c, 1, 0)

    DIM one AS STRING
    one = NCM.GENERATE(h, 1, 3)
    DIM r1() AS STRING
    r1 = SPLIT(one, CHR$(10))
    PRINT "one="; IIF(UBOUND(r1) = 1 AND LEN(r1(0)) > 0, 1, 0)

    DIM neg1 AS STRING, neg2 AS STRING
    neg1 = NCM.GENERATE(h, 2, -1)
    neg2 = NCM.GENERATE(h, 2, -1)
    PRINT "neg="; IIF(LEN(neg1) > 0 AND LEN(neg2) > 0, 1, 0)

    ' ogni parola e' w<id> o s<id> (un pezzo ## a inizio riga perde i cancelletti) del vocabolario di prova
    DIM words() AS STRING, v AS LONG
    words = SPLIT(REPLACE(a, CHR$(10), " "), " ")
    v = 1
    FOR i = 0 TO UBOUND(words)
        IF LEN(words(i)) > 0 AND LEFT$(words(i), 1) <> "w" AND LEFT$(words(i), 1) <> "s" THEN v = 0
    NEXT i
    PRINT "vocab="; v

    NCM.FREEMODEL(h)
END SUB
