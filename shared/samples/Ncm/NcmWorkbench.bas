' FILE: Ncm/NcmWorkbench.bas
' SCENARIO DIDATTICO — un laboratorio sulla generazione di testo "dal grossolano al fine" (NCM, Next-Concept Modeling,
' arXiv 2610.14162) con le quattro funzioni del namespace NCM: LOADMODEL, GENERATE, FREEMODEL, LASTERROR.
'
' NCM non scrive un token alla volta: il testo e' descritto da codici discreti su 4 livelli (da 1 a 8 posizioni nel modello
' di prova), ogni livello viene deciso in UN solo passo del transformer e il livello successivo lo raffina; alla fine un
' decoder riporta i codici ai token. Questo sample usa il modello MINUSCOLO di samples/Fixtures/ncm_tiny (pesi casuali,
' vocabolario da 128 voci): il testo non ha senso, ma tutto il meccanismo e' vero e identico a quello dei modelli reali
' (per quelli, vedi AiLLM/NcmGenerate.bas, 1 GB di pesi).
'
'   A) caricamento e gestione degli errori (LOADMODEL, LASTERROR)
'   B) riproducibilita': stesso seed = stesso testo, seed diversi = testi diversi (GENERATE)
'   C) analisi statistica di un lotto: lunghezza delle righe e parole piu' usate (GENERATE, SPLIT)
'   D) due modelli vivi insieme, liberazione e uso di un handle liberato (FREEMODEL, LASTERROR)
'
' Ogni risultato e' deterministico a parita' di macchina e identico sulle quattro gambe.
' Domini: NCM. Requisiti: libgmb_torch aggiornato (funzioni gmb_ml_ncm_*).
IMPORT NCM

' Conta le righe non vuote di un testo (le righe sono separate da newline)
FUNCTION ContaRighe(t AS STRING) AS LONG
    DIM parts() AS STRING
    DIM i AS LONG, n AS LONG
    parts = SPLIT(t, CHR$(10))
    n = 0
    FOR i = 0 TO UBOUND(parts)
        IF LEN(parts(i)) > 0 THEN n = n + 1
    NEXT i
    RETURN n
END FUNCTION

SUB Main()
    DIM FX AS STRING
    FX = "../Fixtures/ncm_tiny/"
    DIM h AS LONG

    ' ════════════════════════════════════════════════════
    ' A) CARICAMENTO E ERRORI
    ' ════════════════════════════════════════════════════
    PRINT "A) caricamento"
    h = NCM.LOADMODEL("/non/esiste/generator.pt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    IF h = 0 THEN
        PRINT " percorso sbagliato: handle 0, errore dichiarato = "; IIF(LEN(NCM.LASTERROR()) > 0, "si", "no")
    END IF
    h = NCM.LOADMODEL(FX + "generator.pt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    IF h = 0 THEN
        PRINT " caricamento fallito: "; NCM.LASTERROR()
        RETURN
    END IF
    PRINT " modello caricato, handle valido = "; IIF(h > 0, "si", "no")

    ' ════════════════════════════════════════════════════
    ' B) RIPRODUCIBILITA'
    ' ════════════════════════════════════════════════════
    PRINT "B) riproducibilita'"
    DIM a AS STRING, b AS STRING, c AS STRING
    a = NCM.GENERATE(h, 3, 2026)
    b = NCM.GENERATE(h, 3, 2026)
    c = NCM.GENERATE(h, 3, 2027)
    PRINT " righe generate = "; ContaRighe(a)
    PRINT " stesso seed, stesso testo = "; IIF(a = b, "si", "no")
    PRINT " seed diverso, testo diverso = "; IIF(a <> c, "si", "no")
    DIM casuale1 AS STRING, casuale2 AS STRING
    casuale1 = NCM.GENERATE(h, 3, -1)           ' seed negativo: ogni chiamata e' diversa
    casuale2 = NCM.GENERATE(h, 3, -1)
    PRINT " seed negativo (casuale), due chiamate = "; IIF(ContaRighe(casuale1) = 3 AND ContaRighe(casuale2) = 3, "3 righe ciascuna", "errore")

    ' ════════════════════════════════════════════════════
    ' C) ANALISI DI UN LOTTO
    ' ════════════════════════════════════════════════════
    PRINT "C) analisi di un lotto da 32 campioni"
    DIM lotto AS STRING
    lotto = NCM.GENERATE(h, 32, 99)
    DIM righe() AS STRING
    righe = SPLIT(lotto, CHR$(10))
    DIM nRighe AS LONG, paroleTot AS LONG, minP AS LONG, maxP AS LONG
    DIM uso(127) AS LONG                          ' quante volte compare ogni parola w<id> (id 1..99)
    DIM i AS LONG, j AS LONG, np AS LONG, id AS LONG
    DIM parole() AS STRING
    minP = 9999
    maxP = 0
    FOR i = 0 TO UBOUND(righe)
        IF LEN(righe(i)) > 0 THEN
            nRighe = nRighe + 1
            parole = SPLIT(righe(i), " ")
            np = UBOUND(parole) + 1
            paroleTot = paroleTot + np
            IF np < minP THEN minP = np
            IF np > maxP THEN maxP = np
            FOR j = 0 TO UBOUND(parole)
                IF LEFT$(parole(j), 1) = "w" THEN
                    id = VAL(MID$(parole(j), 2))
                    IF id > 0 AND id < 128 THEN uso(id) = uso(id) + 1
                END IF
            NEXT j
        END IF
    NEXT i
    PRINT " righe = "; nRighe; ", parole totali = "; paroleTot
    PRINT " parole per riga: min "; minP; ", max "; maxP; ", media "; FORMAT(paroleTot / nRighe, "0.00")

    ' la parola piu' usata e quante parole diverse sono comparse
    DIM migliore AS LONG, distinte AS LONG
    migliore = 1
    FOR i = 1 TO 127
        IF uso(i) > 0 THEN distinte = distinte + 1
        IF uso(i) > uso(migliore) THEN migliore = i
    NEXT i
    PRINT " parole distinte (w1..w99) = "; distinte; " su 99 possibili"
    PRINT " parola piu' usata = w"; migliore; " ("; uso(migliore); " volte)"

    ' ════════════════════════════════════════════════════
    ' D) DUE MODELLI, LIBERAZIONE, HANDLE NON VALIDO
    ' ════════════════════════════════════════════════════
    PRINT "D) due modelli e liberazione"
    DIM h2 AS LONG
    h2 = NCM.LOADMODEL(FX + "generator.pt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    PRINT " handle diversi = "; IIF(h2 > 0 AND h2 <> h, "si", "no")
    PRINT " stessi pesi e stesso seed, stesso testo = "; IIF(NCM.GENERATE(h2, 3, 2026) = a, "si", "no")
    NCM.FREEMODEL(h)
    DIM morto AS STRING
    morto = NCM.GENERATE(h, 3, 2026)
    PRINT " handle liberato: testo vuoto = "; IIF(LEN(morto) = 0, "si", "no"); ", errore = "; IIF(LEN(NCM.LASTERROR()) > 0, "si", "no")
    PRINT " l'altro modello funziona ancora = "; IIF(ContaRighe(NCM.GENERATE(h2, 3, 5)) = 3, "si", "no")
    NCM.FREEMODEL(h2)
    PRINT "fine"
END SUB
