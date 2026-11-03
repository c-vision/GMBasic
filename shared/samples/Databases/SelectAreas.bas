' Sample completo — AREE DI LAVORO DBF
' Dimostra la semantica richiesta su tutti e quattro i backend:
'   DBF USE ... ALIAS <n>   apre un'area (numerazione 1-based in ordine di apertura)
'   DBF SELECT <alias|n>    COMANDO: commuta l'area corrente
'   DBF.SELECTED([alias|n]) FUNZIONE: numero d'area (0 = nessuna area)
'   DBF CLOSE <alias|n>     chiude SOLO l'area indicata
'   DBF CLOSE               chiude l'area corrente
' Scenario: due tabelle (clienti e ordini), lettura della prima, commutazione,
' chiusura selettiva dell'area NON corrente, chiusura dell'area corrente e riapertura
' che riusa la prima area libera.
' Eseguibile con: tools/sh/parity4 shared/samples/Databases/SelectAreas.bas
IMPORT DBF

TYPE TClienti
    ID AS STRING * 4
    NOME AS STRING * 12
END TYPE

TYPE TOrdini
    CID AS STRING * 4
    DESCR AS STRING * 12
END TYPE

SUB Main()
    ' ── 1. Creazione delle due tabelle ────────────────────────────────────────
    DBF CREATE "/tmp/gmb_sample_sel_clienti.dbf" FROM TYPE TClienti
    DBF APPEND RECORD {ID: "C1", NOME: "Alfa"}
    DBF APPEND RECORD {ID: "C2", NOME: "Beta"}
    DBF CLOSE

    DBF CREATE "/tmp/gmb_sample_sel_ordini.dbf" FROM TYPE TOrdini
    DBF APPEND RECORD {CID: "C1", DESCR: "Ordine-1"}
    DBF APPEND RECORD {CID: "C2", DESCR: "Ordine-2"}
    DBF APPEND RECORD {CID: "C1", DESCR: "Ordine-3"}
    DBF CLOSE

    ' ── 2. Apertura di due aree: Clienti = area 1, Ordini = area 2 ───────────
    DBF USE "/tmp/gmb_sample_sel_clienti.dbf" ALIAS Clienti
    DBF USE "/tmp/gmb_sample_sel_ordini.dbf" ALIAS Ordini

    PRINT "aree aperte: clienti="; DBF.SELECTED("Clienti"); " ordini="; DBF.SELECTED("Ordini")
    PRINT "corrente dopo USE: "; DBF.ALIAS(); " area="; DBF.SELECTED()

    ' ── 3. DBF SELECT è un COMANDO: cambia l'area corrente ───────────────────
    DBF SELECT Clienti
    PRINT "dopo SELECT Clienti: "; DBF.ALIAS(); " area="; DBF.SELECTED()

    ' Lettura dei record dell'area corrente (Clienti).
    DIM i AS INTEGER
    FOR i = 1 TO DBF.LASTREC()
        DBF GO i
        PRINT "cliente: "; TRIM(ID); " / "; TRIM(NOME)
    NEXT i

    ' ── 4. Commutazione sull'altra area e lettura ────────────────────────────
    DBF SELECT Ordini
    PRINT "dopo SELECT Ordini: "; DBF.ALIAS(); " area="; DBF.SELECTED()
    FOR i = 1 TO DBF.LASTREC()
        DBF GO i
        PRINT "ordine: "; TRIM(CID); " / "; TRIM(DESCR)
    NEXT i

    ' ── 5. SELECTED(alias) non cambia l'area: è una FUNZIONE ─────────────────
    PRINT "area di Clienti: "; DBF.SELECTED("Clienti")
    PRINT "area inesistente: "; DBF.SELECTED("Nessuna")

    ' ── 6. DBF CLOSE <alias> chiude SOLO quell'area, non la corrente ─────────
    DBF CLOSE Clienti
    PRINT "dopo CLOSE Clienti (non corrente): "; DBF.ALIAS(); " area="; DBF.SELECTED()
    PRINT "Clienti ancora aperta? "; DBF.SELECTED("Clienti")

    ' ── 7. DBF CLOSE nudo chiude l'area corrente (Ordini) ────────────────────
    DBF CLOSE
    PRINT "dopo CLOSE corrente: area="; DBF.SELECTED()

    ' ── 8. Riapertura: riusa la prima area libera ────────────────────────────
    DBF USE "/tmp/gmb_sample_sel_clienti.dbf" ALIAS Clienti
    PRINT "riaperta Clienti: area="; DBF.SELECTED(); " lastrec="; DBF.LASTREC()
    DBF CLOSE
END SUB
