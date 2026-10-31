' Sample AI OBSERVE — registro osservazioni con ContextObject.
' Dimostra: OBSERVE FILE/LOG, OBSERVATION* letture, AI.ASK con observation.
IMPORT AI
IMPORT IO

SUB Main()
    PRINT "=== AI OBSERVE demo ==="
    PRINT ""

    ' 1. OBSERVE FILE — registra un file con hash FNV-1a64
    DIM f$ = "../../../samples/Fixtures/ai/fattura_it.txt"
    AI OBSERVE FILE f$ INTO fid
    PRINT "1. FILE osservato, id=" + STR$(fid)
    PRINT "   hash=" + AI.OBSERVATIONHASH$(fid)
    PRINT "   path=" + AI.OBSERVATIONFILE$(fid)

    ' 2. OBSERVE LOG — registra le ultime N righe di un file
    AI OBSERVE LOG f$ INTO lid
    PRINT "2. LOG osservato, id=" + STR$(lid)

    ' 3. OBSERVATIONLATEST — ultimo id registrato
    DIM latest$ = AI.OBSERVATIONLATEST$()
    PRINT "3. Ultimo id: " + latest$

    ' 4. OBSERVATIONCOUNT — conteggio totale
    DIM n = AI.OBSERVATIONCOUNT()
    PRINT "4. Conteggio: " + STR$(n)

    ' 5. OBSERVATION — dettaglio JSON
    DIM o$ = AI.OBSERVATION$(lid)
    PRINT "5. JSON dettaglio: " + LEFT$(o$, 100) + "..."

    ' 6. OBSERVATIONLIST — elenco sommario
    DIM l$ = AI.OBSERVATIONLIST$()
    PRINT "6. Lista: " + LEFT$(l$, 80) + "..."

    ' 7. OBSERVATIONFILE — path dell'artefatto
    DIM p$ = AI.OBSERVATIONFILE$(fid)
    PRINT "7. Path: " + p$

    PRINT ""

    PRINT "=== Fine demo ==="
    PRINT "Totale osservazioni: " + STR$(AI.OBSERVATIONCOUNT())



    ' 8. OBSERVE SCREEN — cattura schermo e la registra come ContextObject (P1).
    ' La cartella di uscita è creata dal runtime (come fa il percorso C con mkdir -p) e l'esito è
    ' ESPLICITO: se la cattura fallisce (permessi di registrazione schermo non concessi alla
    ' macchina) il comando solleva un errore catturabile, quindi il sample resta eseguibile e
    ' deterministico anche su una macchina senza permessi.
    DIM cBefore AS DOUBLE
    DIM cAfter AS DOUBLE
    ON ERROR GOTO ScreenKo
    cBefore = AI.OBSERVATIONCOUNT()
    AI OBSERVE SCREEN "work/observe_screen.png"
    cAfter = AI.OBSERVATIONCOUNT()
    IF cAfter = cBefore + 1 THEN
        PRINT "8. OBSERVE SCREEN: registrata"
    ELSE
        PRINT "8. OBSERVE SCREEN: non registrata"
    END IF
    GOTO ScreenEnd
ScreenKo:
    PRINT "8. OBSERVE SCREEN: cattura non disponibile (errore esplicito catturato)"
ScreenEnd:

END SUB
