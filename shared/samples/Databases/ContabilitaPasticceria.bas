' FILE: Databases/ContabilitaPasticceria.bas
' SCENARIO DIDATTICO — la prima registrazione contabile di una pasticceria.
'
' Il titolare apre un libro giornale (LEDGER) e registra un mese di attività in
' PARTITA DOPPIA: ogni movimento ha almeno due righe e la somma deve essere zero
' (quello che entra in un conto esce da un altro). Il programma:
'   1) registra apertura cassa, vendite e acquisti;
'   2) verifica che ogni transazione sia bilanciata (VALIDATE);
'   3) fissa un BUDGET per la farina e legge lo scostamento (VARIANCE);
'   4) legge il piano dei conti, il registro e i movimenti di una transazione;
'   5) importa gli ultimi movimenti dalla banca (IMPORT da CSV) e salva.
'
' Le correzioni (LEDGER DELETE/EDIT) non sono ancora affidabili nel runtime: vedi
' Bugs/(open) - LEDGER-Delete-Edit-Non-Aggiornano.md. Il Sample quindi non le insegna.
'
' Domini: LEDGER + IO + MATH. File di lavoro in work/ (cancellati a fine scenario).
IMPORT LEDGER
IMPORT IO

SUB Main()
    MKDIR "work"
    ON ERROR RESUME NEXT
    KILL "work/pasticceria.dat"
    KILL "work/pasticceria.idx"
    KILL "work/banca.csv"
    ON ERROR GOTO 0

    ' ── 0. Estratto conto della banca da importare ───────────────────────────
    OPEN "work/banca.csv" FOR WRITE AS #1
    WRITE #1, "2026-02-03,Incasso POS,850.00"
    WRITE #1, "2026-02-05,Fornitore zucchero,-120.00"
    CLOSE #1

    ' ── 1. Apertura del libro giornale ───────────────────────────────────────
    DIM h AS LONG
    h = LEDGER.OPEN("work/pasticceria.dat")
    DIM id AS LONG

    PRINT "== 1. Movimenti del mese =="
    ' Apertura: la cassa (attivo, positivo) è finanziata dal patrimonio (negativo).
    LEDGER ADD h WITH "2026-01-01", "Apertura cassa" { _
        "Attivita:Cassa": 2000.00, "Patrimonio:Apertura": -2000.00 } INTO id
    PRINT "apertura: transazione "; id

    ' Vendita al banco: entra cassa, escono ricavi (segno negativo).
    LEDGER ADD h WITH "2026-01-10", "Vendita banco" { _
        "Attivita:Cassa": 900.00, "Ricavi:Vendite": -900.00 } INTO id
    PRINT "vendita:  transazione "; id

    ' Acquisto di farina: entra un costo, esce cassa.
    LEDGER ADD h WITH "2026-01-12", "Acquisto farina" { _
        "Spese:Farina": 300.00, "Attivita:Cassa": -300.00 } INTO id
    PRINT "acquisto: transazione "; id

    PRINT "transazioni registrate: "; LEDGER.COUNT(h)
    PRINT "ultimo numero: "; LEDGER.LASTREC(h)

    ' ── 2. Ogni transazione deve essere bilanciata ───────────────────────────
    PRINT "== 2. Verifica della partita doppia =="
    PRINT "libro bilanciato: "; LEDGER.VALIDATE(h)

    ' ── 3. Budget e scostamento ──────────────────────────────────────────────
    PRINT "== 3. Budget della farina =="
    ' 500 EUR previsti per gennaio: con 300 spesi restano 200 di margine.
    LEDGER BUDGET h, "Spese:Farina", 500.00, "2026-01"
    DIM scostamento AS DOUBLE
    scostamento = LEDGER.VARIANCE(h, "Spese:Farina", "2026-01")
    PRINT "budget: 500.00 EUR, speso: 300.00 EUR"
    PRINT "scostamento: "; MATH.ROUND(scostamento, 2); " EUR"
    IF scostamento > 0 THEN
        PRINT "esito: sotto budget"
    ELSE
        PRINT "esito: budget superato"
    END IF

    ' ── 4. Lettura del libro ─────────────────────────────────────────────────
    PRINT "== 4. Piano dei conti =="
    DIM conti() = LEDGER.ACCOUNTS(h)
    DIM i AS LONG
    FOR i = 0 TO LEN(conti) - 1
        PRINT "  - "; conti(i)
    NEXT i
    PRINT "conti con saldo: "; LEN(LEDGER.BALANCE(h))
    PRINT "movimenti in registro: "; LEN(LEDGER.REGISTER(h))
    ' GET restituisce le righe (posting) di una singola transazione.
    PRINT "righe dell'ultima transazione: "; LEN(LEDGER.GET(h, LEDGER.LASTREC(h)))

    ' ── 5. Import dei movimenti bancari ──────────────────────────────────────
    ' IMPORT applica la mappatura date,desc,amount del CSV: 0 = import riuscito.
    PRINT "== 5. Import banca =="
    PRINT "esito import (0 = ok): "; LEDGER.IMPORT(h, "work/banca.csv", "date,desc,amount")
    PRINT "transazioni totali: "; LEDGER.COUNT(h)

    ' ── 6. Grafico e chiusura ────────────────────────────────────────────────
    ' PLOT crea il grafico dei saldi e restituisce un identificativo (>0 se ok).
    PRINT "== 6. Salvataggio =="
    PRINT "grafico creato: "; (LEDGER.PLOT(h, "", "bar") > 0)
    LEDGER SAVE h
    LEDGER CLOSE h

    KILL "work/pasticceria.dat"
    KILL "work/pasticceria.idx"
    KILL "work/banca.csv"
    PRINT "Chiusura del mese completata."
END SUB
