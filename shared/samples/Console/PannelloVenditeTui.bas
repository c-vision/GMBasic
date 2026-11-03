' FILE: Console/PannelloVenditeTui.bas
' SCENARIO DIDATTICO — pannello di controllo in TUI (text user interface).
'
' La TUI è l'interfaccia "a caratteri" dei programmi da terminale: si compone una griglia
' di celle con bordi, si colora il testo e si stampano righe già allineate. Non serve un
' toolkit grafico e funziona identica in SSH, in CI e sul terminale locale.
'
' Il programma costruisce il pannello delle vendite di un negozio:
'   1) intestazione in un riquadro a doppia riga;
'   2) tabella allineata (nomi a sinistra, importi a destra);
'   3) quota di fatturato con barra a blocchi;
'   4) riga di stato (totale) colorata per soglia.
'
' Nota pratica: le righe vengono COSTRUITE come stringhe e stampate in ordine, non
' posizionate con LOCATE. Così il pannello è identico quando l'output è rediretto su file
' o pipe (CI, test di parità) e quando è su un terminale.
'
' Domini: CONSOLE (COLOR) + stringhe + MATH. Output deterministico, nessun input.
IMPORT CONSOLE
IMPORT MATH

' Riga interna del riquadro: "║" + testo allineato a sinistra + "║".
FUNCTION RigaBox(testo AS STRING, larghezza AS LONG) AS STRING
    DIM s AS STRING
    s = testo
    DO WHILE LEN(s) < larghezza - 2
        s = s + " "
    LOOP
    IF LEN(s) > larghezza - 2 THEN s = LEFT$(s, larghezza - 2)
    RETURN "║" + s + "║"
END FUNCTION

' Riga di tabella: nome a sinistra, numero a destra, larghezza totale fissa.
FUNCTION RigaTabella(nome AS STRING, valore AS STRING, larghezza AS LONG) AS STRING
    DIM s AS STRING
    s = LEFT$(nome + SPACE(20), 20) + "│"
    s = s + " " + RIGHT$(SPACE(12) + valore, 12) + " │ "
    RETURN "║" + LEFT$(s + SPACE(larghezza - 2), larghezza - 2) + "║"
END FUNCTION

' Barra a blocchi: 12 celle (la larghezza della colonna "andamento").
FUNCTION Barra(percentuale AS DOUBLE) AS STRING
    DIM pieni AS LONG
    DIM i AS LONG
    DIM s AS STRING
    pieni = INT(percentuale * 12.0 / 100.0 + 0.5)
    IF pieni < 0 THEN pieni = 0
    IF pieni > 12 THEN pieni = 12
    s = ""
    FOR i = 1 TO 12
        IF i <= pieni THEN
            s = s + "█"
        ELSE
            s = s + "·"
        END IF
    NEXT i
    RETURN s
END FUNCTION

SUB Main()
    DIM larghezza AS LONG
    larghezza = 60

    ' ── 1. Intestazione ──────────────────────────────────────────────────────
    COLOR 15, 4
    PRINT "╔" + STRING$(larghezza - 2, "═") + "╗"
    PRINT RigaBox(" PANNELLO VENDITE — Pasticceria Rossi", larghezza)
    PRINT RigaBox(" turno del mattino", larghezza)
    PRINT "╚" + STRING$(larghezza - 2, "═") + "╝"
    COLOR 7, 0

    ' ── 2. Tabella dei reparti ───────────────────────────────────────────────
    DIM nomi(3) AS STRING
    DIM vendite(3) AS DOUBLE
    nomi(0) = "Pasticceria" : vendite(0) = 4200
    nomi(1) = "Pane"        : vendite(1) = 2600
    nomi(2) = "Caffetteria" : vendite(2) = 3200
    nomi(3) = "Gelateria"   : vendite(3) = 0

    DIM totale AS DOUBLE
    DIM i AS LONG
    totale = 0
    FOR i = 0 TO 3
        totale = totale + vendite(i)
    NEXT i

    PRINT "╔" + STRING$(larghezza - 2, "═") + "╗"
    COLOR 11, 0
    PRINT RigaTabella("REPARTO", "VENDITE", larghezza)
    COLOR 7, 0
    PRINT "╟" + STRING$(larghezza - 2, "─") + "╢"

    FOR i = 0 TO 3
        DIM quota AS DOUBLE
        quota = 0
        IF totale > 0 THEN quota = vendite(i) / totale * 100.0
        PRINT RigaTabella(nomi(i), TRIM$(STR$(vendite(i))), larghezza)
        PRINT RigaTabella("  quota " + TRIM$(STR$(MATH.ROUND(quota, 1))) + "%", Barra(quota), larghezza)
    NEXT i

    ' ── 3. Riga di stato colorata per soglia ─────────────────────────────────
    PRINT "╠" + STRING$(larghezza - 2, "═") + "╣"
    IF totale > 10000 THEN
        COLOR 10, 0
        PRINT RigaBox(" TOTALE " + TRIM$(STR$(totale)) + " EUR — obiettivo raggiunto", larghezza)
    ELSE
        COLOR 12, 0
        PRINT RigaBox(" TOTALE " + TRIM$(STR$(totale)) + " EUR — sotto obiettivo", larghezza)
    END IF
    COLOR 7, 0
    PRINT "╚" + STRING$(larghezza - 2, "═") + "╝"

    PRINT "Pannello completato."
END SUB
