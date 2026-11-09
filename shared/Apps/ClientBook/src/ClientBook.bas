' FILE: shared/Apps/ClientBook/src/ClientBook.bas
' ============================================================================
' ClientBook — anagrafica clienti con controlli Clipper e database DBF.
'
' Campi: Cognome, Nome, Data di nascita (DatePicker), Sesso (CheckBox),
'        Paese di nascita (ComboBox con elenco).
' Operazioni con PULSANTI: Aggiungi · Modifica · Elimina · Esci.
' L'archivio è un DBF accanto all'ESEGUIBILE (`IO.BINPATH()/clienti.dbf`, cioè
' `<progetto>/bin/`): vive fuori dal repository ed è ricreato al primo avvio.
'
' MODALITA' UI — argomenti del PROGRAMMA compilato (main(argc,argv) → COMMAND$):
'   -c   SystemConsole   (console di sistema, output diretto)
'   -t   StandardConsole (terminale standard: modalità predefinita)
'   -s   SdlConsole      (console TUI su superficie SDL)
'   -g   Graphic         (pagina grafica: controlli pixel nativi)
'   (nessun parametro)   = -t
'   --selftest           esegue il ciclo CRUD senza aprire la UI (verifica headless)
'
' Il layout è UNO SOLO, in celle: in grafica la clausola `USE CELLS` di SCREEN
' converte x/y/w/h nel rettangolo pixel, così TUI e GUI mostrano la stessa
' schermata e la stessa disposizione dei controlli.
'
' Compilazione ed esecuzione (dalla root del monorepo): il compilatore mette
' l'eseguibile in <progetto>/bin e lo marca eseguibile (0755).
'   ./bin/gmb build --project shared/Apps/ClientBook --entry src/ClientBook.bas
'   ./shared/Apps/ClientBook/bin/ClientBook -g
'   ./shared/Apps/ClientBook/bin/ClientBook --selftest
' ============================================================================
IMPORT DBF
IMPORT CONTROLS
IMPORT IO
IMPORT STRINGS

' ---- schema dell'archivio --------------------------------------------------
TYPE TCliente
    Cognome AS STRING * 30
    Nome AS STRING * 30
    Nascita AS STRING * 10      ' gg/mm/aaaa (componenti nel DatePicker)
    Sesso AS STRING * 1         ' "M" / "F"
    Paese AS STRING * 40
END TYPE

' ---- stato globale ---------------------------------------------------------
DIM g_appDir AS STRING
DIM g_binDir AS STRING
DIM g_dbPath AS STRING
DIM g_mode AS STRING
DIM g_gfx AS INTEGER            ' 1 = pagina grafica
DIM g_quit AS INTEGER
DIM g_demo AS INTEGER
DIM g_seed AS INTEGER          ' 1 = archivio di esempio al primo avvio

' valori del form (sorgente unica: letti/scritti dai gestori)
DIM g_cognome AS STRING
DIM g_nome AS STRING
DIM g_paese AS STRING            ' nome del paese (derivato dall'indice)
DIM g_paeseIdx AS INTEGER       ' indice scelto nella combobox
DIM g_sesso AS INTEGER          ' 1 = Maschio, 0 = Femmina
DIM g_dn AS INTEGER
DIM g_mn AS INTEGER
DIM g_yn AS INTEGER
DIM g_sel AS INTEGER            ' riga scelta nella lista
DIM g_loaded AS INTEGER         ' ultima riga caricata nel form
DIM g_count AS INTEGER          ' clienti presenti
DIM g_status AS STRING
DIM g_rows AS STRING            ' righe della lista separate da "|"
DIM g_dummy AS STRING           ' INTO del datepicker (one-shot: si leggono le proprietà)

DIM g_paesi(19) AS STRING
DIM g_npaesi AS INTEGER

' ============================================================================
SUB Main()
    BootPaths()
    InitPaesi()
    SelectMode()                       ' -c / -t / -s / -g (default -t)

    IF HasArg("--selftest") THEN
        RunSelfTest()
        EXIT SUB
    END IF

    g_seed = 1
    g_demo = HasArg("--demo")

    DataOpen()
    BuildUi()
    RefreshList()
    IF g_count > 0 THEN CALL LoadRow(0)     ' form coerente con la riga selezionata

    ' --demo: qualche frame (repaint + input) e uscita, senza dipendere da mouse/tastiera.
    ' Serve a smoke-test e CI: non entra nel ciclo interattivo.
    IF g_demo THEN
        DIM k AS INTEGER
        FOR k = 1 TO 3
            DOEVENTS
        NEXT k
        IF g_gfx = 1 THEN
            SAVE SCREEN g_binDir + "/clientbook.png"
            PRINT "DEMO-PNG="; g_binDir + "/clientbook.png"
        END IF
        DBF CLOSE
        EXIT SUB
    END IF

    EventLoop()
    DBF CLOSE
END SUB

' ---------------------------------------------------------------------------
' Percorsi: il compilato vive in <progetto>/bin (cfr. `gmb build`), e i dati
' dell'app stanno ACCANTO all'eseguibile. IO.BINPATH() è la cartella del binario
' in esecuzione: non dipende dalla directory da cui lo si lancia.
' ---------------------------------------------------------------------------
SUB BootPaths()
    DIM binDir AS STRING, appDir AS STRING
    binDir = IO.BINPATH()
    IF LEN(binDir) = 0 THEN binDir = IO.CWD()
    g_binDir = RTrimSlash(binDir)
    appDir = IO.PARENTDIR(g_binDir)
    IF LEN(appDir) = 0 THEN appDir = g_binDir
    g_appDir = RTrimSlash(appDir)
    IF LEN(g_binDir) = 0 THEN g_binDir = "bin"
    IF NOT IO.DIREXISTS(g_binDir) THEN MKDIR g_binDir
    g_dbPath = g_binDir + "/clienti.dbf"
END SUB

FUNCTION RTrimSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    DO WHILE LEN(s) > 0
        IF RIGHT(s, 1) = "/" OR RIGHT(s, 1) = "\" THEN
            s = LEFT(s, LEN(s) - 1)
        ELSE
            EXIT DO
        END IF
    LOOP
    RTrimSlash = s
END FUNCTION

SUB InitPaesi()
    g_paesi(0) = "Italia"
    g_paesi(1) = "Francia"
    g_paesi(2) = "Germania"
    g_paesi(3) = "Spagna"
    g_paesi(4) = "Portogallo"
    g_paesi(5) = "Belgio"
    g_paesi(6) = "Olanda"
    g_paesi(7) = "Svizzera"
    g_paesi(8) = "Austria"
    g_paesi(9) = "Grecia"
    g_paesi(10) = "Regno Unito"
    g_paesi(11) = "Irlanda"
    g_paesi(12) = "Svezia"
    g_paesi(13) = "Polonia"
    g_paesi(14) = "Stati Uniti"
    g_paesi(15) = "Canada"
    g_paesi(16) = "Brasile"
    g_paesi(17) = "Argentina"
    g_paesi(18) = "Giappone"
    g_paesi(19) = "Australia"
    g_npaesi = 20
END SUB

' ---------------------------------------------------------------------------
' Modalità UI dalla riga di comando: COMMAND$(1) (BASIC non ha argv, ma
' COMMAND$ sì: è la superficie SYSTEM.COMMAND).
' ---------------------------------------------------------------------------
SUB SelectMode()
    DIM arg AS STRING
    arg = "-t"
    IF COMMANDCOUNT() >= 1 THEN arg = LCASE(COMMAND$(1))
    g_gfx = 0
    SELECT CASE arg
        CASE "-c", "c", "system", "--system"
            g_mode = "-c (SystemConsole)"
            TEXTMODE %SYSTEM
            SCREEN 0
        CASE "-s", "s", "sdl", "--sdl"
            g_mode = "-s (SdlConsole)"
            TEXTMODE %SDL
            SCREEN 0
        CASE "-g", "g", "graphic", "--graphic"
            g_mode = "-g (Graphic)"
            g_gfx = 1
            ' Pagina grafica con coordinate DICHIARATE in celle: un solo layout
            ' per TUI e GUI (il frame moltiplica per la cella del font UI).
            SCREEN 12, 900, 700, 32, CELLS
            WINDOW THEME "glass"
        CASE ELSE
            g_mode = "-t (StandardConsole)"
            TEXTMODE %ANSI
            SCREEN 0
    END SELECT
    g_status = "Modalità " + g_mode
END SUB

FUNCTION HasArg(want AS STRING) AS INTEGER
    DIM i AS INTEGER
    HasArg = 0
    FOR i = 1 TO COMMANDCOUNT()
        IF LCASE(COMMAND$(i)) = LCASE(want) THEN HasArg = 1
    NEXT i
END FUNCTION

' ---------------------------------------------------------------------------
' UI: griglia di celle comune a tutte le modalità (vedi USE CELLS in SCREEN).
' ---------------------------------------------------------------------------
SUB BuildUi()
    ' Il layout sta in 24 righe (griglia della console TUI): in grafica la
    ' clausola USE CELLS lo porta in pixel con la stessa disposizione.
    LABEL 2, 0, 60, "ANAGRAFICA CLIENTI — archivio DBF" NAME "lblTitolo"

    ' campi del cliente
    LABEL 2, 2, 16, "Cognome"
    TEXTBOX 20, 2, 34, 2 INTO g_cognome NAME "txtCognome"
    LABEL 2, 4, 16, "Nome"
    TEXTBOX 20, 4, 34, 2 INTO g_nome NAME "txtNome"
    LABEL 2, 6, 16, "Data di nascita"
    DATEPICKER 20, 6 INTO g_dummy NAME "dpNascita"
    ' Il datepicker nasce 22x24 celle (altezza = area del calendario): qui serve
    ' solo il CAMPO, alto una riga e largo quanto la data.
    CONTROL SET "dpNascita", "width", 14
    CONTROL SET "dpNascita", "height", 2
    LABEL 2, 8, 16, "Sesso (Maschio)"
    CHECKBOX 20, 8, 20, "Maschio" INTO g_sesso NAME "chkSesso"
    LABEL 2, 10, 16, "Paese di nascita"
    COMBOBOX 20, 10, 30, "Italia|Francia|Germania|Spagna|Portogallo|Belgio|Olanda|Svizzera|Austria|Grecia|Regno Unito|Irlanda|Svezia|Polonia|Stati Uniti|Canada|Brasile|Argentina|Giappone|Australia" INTO g_paeseIdx NAME "cmbPaese"

    ' elenco clienti
    LABEL 2, 12, 70, "Clienti — seleziona una riga, poi premi Modifica o Elimina"
    LISTBOX 2, 13, 74, 7, "" INTO g_sel NAME "lstClienti" ON SELCHANGE OnPick

    ' pulsanti delle operazioni
    BUTTON 2, 20, 16, 2, "Aggiungi" ON CLICK OnAdd NAME "btnAdd"
    BUTTON 20, 20, 16, 2, "Modifica" ON CLICK OnEdit NAME "btnEdit"
    BUTTON 38, 20, 16, 2, "Elimina" ON CLICK OnDelete NAME "btnDel"
    BUTTON 56, 20, 16, 2, "Esci" ON CLICK OnQuit NAME "btnQuit"

    STATUSBAR 2, 23, 74, "" NAME "sbStato"
    CALL SetStatus()
END SUB

' ---------------------------------------------------------------------------
' Archivio
' ---------------------------------------------------------------------------
SUB DataOpen()
    DIM nuovo AS INTEGER
    nuovo = 0
    IF NOT IO.FILEEXISTS(g_dbPath) THEN
        DBF CREATE g_dbPath FROM TYPE TCliente
        nuovo = 1
    END IF
    ' UNSAFE: app desktop mono-utente. EXCLUSIVE lascia `<db>.wal.xlock` e un processo
    ' terminato male (CI, chiusura forzata) bloccherebbe il successivo all'apertura.
    DBF USE g_dbPath ALIAS CLIENTI UNSAFE
    DBF SET DELETED ON
    IF nuovo AND g_seed THEN CALL Seed()
END SUB

' Archivio di esempio al primo avvio (archivio appena creato): mostra subito
' la forma dei dati senza obbligare a inserire tutto a mano.
SUB Seed()
    DIM saveC AS STRING, saveN AS STRING, saveP AS INTEGER, saveS AS INTEGER
    DIM saveD AS INTEGER, saveM AS INTEGER, saveY AS INTEGER
    saveC = g_cognome : saveN = g_nome : saveP = g_paeseIdx : saveS = g_sesso
    saveD = g_dn : saveM = g_mn : saveY = g_yn

    g_cognome = "Rossi" : g_nome = "Mario" : g_sesso = 1 : g_paeseIdx = 0
    g_dn = 15 : g_mn = 3 : g_yn = 1980
    CALL AppendCurrent()
    g_cognome = "Bianchi" : g_nome = "Anna" : g_sesso = 0 : g_paeseIdx = 1
    g_dn = 2 : g_mn = 11 : g_yn = 1992
    CALL AppendCurrent()
    g_cognome = "Schmidt" : g_nome = "Lukas" : g_sesso = 1 : g_paeseIdx = 2
    g_dn = 27 : g_mn = 6 : g_yn = 1975
    CALL AppendCurrent()

    g_cognome = saveC : g_nome = saveN : g_paeseIdx = saveP : g_sesso = saveS
    g_dn = saveD : g_mn = saveM : g_yn = saveY
END SUB

FUNCTION CountRecs() AS INTEGER
    CountRecs = DBF.LASTREC()
END FUNCTION

' Posiziona il cursore sulla riga n (0-based) dell'elenco.
SUB GoRow(n AS INTEGER)
    DIM i AS INTEGER
    IF n < 0 THEN EXIT SUB
    DBF GO TOP
    FOR i = 1 TO n
        IF DBF.EOF() THEN EXIT SUB
        DBF SKIP 1
    NEXT i
END SUB

' Riga dell'elenco: "Cognome Nome · gg/mm/aaaa · M · Paese"
SUB RefreshList()
    DIM row AS STRING, sep AS STRING
    g_rows = ""
    g_count = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        ' Campi letti per NOME sul record corrente (late binding DBF, parità 4 gambe):
        ' `DBF GET <struttura>` non è emesso dalla gamba C99Native.
        row = TRIM(Cognome) + " " + TRIM(Nome)
        row = row + " · " + TRIM(Nascita)
        row = row + " · " + TRIM(Sesso)
        row = row + " · " + TRIM(Paese)
        IF g_count > 0 THEN sep = "|" ELSE sep = ""
        g_rows = g_rows + sep + row
        g_count = g_count + 1
        DBF SKIP 1
    LOOP
    CONTROL SET "lstClienti", "items", 0, g_rows
    CONTROL SET "lstClienti", "value", 0
    g_sel = 0
    g_loaded = -1
    CALL SetStatus()
END SUB

SUB SetStatus()
    CONTROL SET "sbStato", "text", 0, g_status
END SUB

' ---------------------------------------------------------------------------
' Date: componenti -> "gg/mm/aaaa" e ritorno.
' ---------------------------------------------------------------------------
FUNCTION Pad2(n AS INTEGER) AS STRING
    Pad2 = PADL(LTRIM(STR(n)), 2, "0")
END FUNCTION

FUNCTION Pad4(n AS INTEGER) AS STRING
    Pad4 = PADL(LTRIM(STR(n)), 4, "0")
END FUNCTION

FUNCTION DateText() AS STRING
    DateText = Pad2(g_dn) + "/" + Pad2(g_mn) + "/" + Pad4(g_yn)
END FUNCTION

SUB DateFromText(s AS STRING)
    g_dn = VAL(MID(s, 1, 2))
    g_mn = VAL(MID(s, 4, 2))
    g_yn = VAL(MID(s, 7, 4))
    IF g_dn < 1 THEN g_dn = 1
    IF g_mn < 1 THEN g_mn = 1
    IF g_yn < 1 THEN g_yn = 1900
END SUB

FUNCTION PaeseIndex(nome AS STRING) AS INTEGER
    DIM i AS INTEGER
    PaeseIndex = 0
    FOR i = 0 TO g_npaesi - 1
        IF UCASE(g_paesi(i)) = UCASE(TRIM(nome)) THEN
            PaeseIndex = i
            EXIT FUNCTION
        END IF
    NEXT i
END FUNCTION

' ---------------------------------------------------------------------------
' Form <-> modello
' ---------------------------------------------------------------------------
SUB ReadForm()
    CONTROL GETS "txtCognome", "text" INTO g_cognome
    CONTROL GETS "txtNome", "text" INTO g_nome
    CONTROL GET "chkSesso", "value" INTO g_sesso
    CONTROL GET "cmbPaese", "value" INTO g_paeseIdx
    IF g_paeseIdx < 0 THEN g_paeseIdx = 0
    IF g_paeseIdx >= g_npaesi THEN g_paeseIdx = g_npaesi - 1
    g_paese = g_paesi(g_paeseIdx)
    CONTROL GET "dpNascita", "day" INTO g_dn
    CONTROL GET "dpNascita", "month" INTO g_mn
    CONTROL GET "dpNascita", "year" INTO g_yn
END SUB

SUB WriteForm()
    CONTROL SET "txtCognome", "text", 0, g_cognome
    CONTROL SET "txtNome", "text", 0, g_nome
    CONTROL SET "dpNascita", "day", g_dn
    CONTROL SET "dpNascita", "month", g_mn
    CONTROL SET "dpNascita", "year", g_yn
    CONTROL SET "chkSesso", "value", g_sesso
    CONTROL SET "cmbPaese", "value", g_paeseIdx
END SUB

SUB ClearForm()
    g_cognome = ""
    g_nome = ""
    g_paeseIdx = 0
    g_paese = g_paesi(0)
    g_sesso = 0
    g_dn = 1
    g_mn = 1
    g_yn = 1990
    CALL WriteForm()
END SUB

' Carica nel form il cliente della riga selezionata.
SUB LoadRow(n AS INTEGER)
    IF n < 0 OR n >= g_count THEN EXIT SUB
    CALL GoRow(n)
    IF DBF.EOF() THEN EXIT SUB
    g_cognome = TRIM(Cognome)
    g_nome = TRIM(Nome)
    g_sesso = 0
    IF TRIM(Sesso) = "M" THEN g_sesso = 1
    CALL DateFromText(TRIM(Nascita))
    g_paeseIdx = PaeseIndex(Paese)
    g_paese = g_paesi(g_paeseIdx)
    CALL WriteForm()
    g_loaded = n
END SUB

SUB AppendCurrent()
    DBF APPEND BLANK
    CALL StoreFields()
    g_count = CountRecs()
END SUB

SUB StoreFields()
    DIM sessoTxt AS STRING, paeseTxt AS STRING
    IF g_sesso <> 0 THEN sessoTxt = "M" ELSE sessoTxt = "F"
    paeseTxt = g_paesi(g_paeseIdx)
    DBF REPLACE Cognome WITH g_cognome, Nome WITH g_nome, Nascita WITH DateText(), Sesso WITH sessoTxt, Paese WITH paeseTxt
END SUB

SUB SaveRow(n AS INTEGER)
    IF n < 0 OR n >= g_count THEN EXIT SUB
    CALL GoRow(n)
    IF DBF.EOF() THEN EXIT SUB
    CALL StoreFields()
END SUB

SUB DeleteRow(n AS INTEGER)
    IF n < 0 OR n >= g_count THEN EXIT SUB
    CALL GoRow(n)
    IF DBF.EOF() THEN EXIT SUB
    DBF DELETE
    DBF PACK
    g_count = CountRecs()
END SUB

' ---------------------------------------------------------------------------
' Gestori dei pulsanti
' ---------------------------------------------------------------------------
SUB OnPick()
    ' In grafica il cambio selezione è un evento (SELCHANGE): carica il cliente.
    CALL LoadRow(g_sel)
    g_status = "Cliente caricato: " + g_cognome + " " + g_nome
    CALL SetStatus()
END SUB

SUB OnAdd()
    CALL ReadForm()
    IF LEN(TRIM(g_cognome)) = 0 AND LEN(TRIM(g_nome)) = 0 THEN
        g_status = "Aggiungi: inserire almeno cognome o nome"
        CALL SetStatus()
        EXIT SUB
    END IF
    CALL AppendCurrent()
    CALL RefreshList()
    CALL ClearForm()
    g_status = "Cliente aggiunto (totale " + LTRIM(STR(g_count)) + ")"
    CALL SetStatus()
END SUB

SUB OnEdit()
    CALL ReadForm()
    CALL SaveRow(g_sel)
    CALL RefreshList()
    CALL LoadRow(g_sel)
    g_status = "Cliente modificato"
    CALL SetStatus()
END SUB

SUB OnDelete()
    IF g_count = 0 THEN
        g_status = "Elimina: archivio vuoto"
        CALL SetStatus()
        EXIT SUB
    END IF
    CALL DeleteRow(g_sel)
    CALL RefreshList()
    CALL ClearForm()
    g_status = "Cliente eliminato (totale " + LTRIM(STR(g_count)) + ")"
    CALL SetStatus()
END SUB

SUB OnQuit()
    g_quit = 1
END SUB

' ---------------------------------------------------------------------------
' Ciclo eventi: nessun timer e nessun ridisegno manuale. DOEVENTS consegna
' input + repaint incrementale; il cambio scheda/selezione arriva come evento.
' In TUI il mouse non genera SELCHANGE: si confronta la selezione (una lettura
' per frame) e si carica il cliente quando cambia.
' ---------------------------------------------------------------------------
SUB EventLoop()
    DO WHILE g_quit = 0
        DOEVENTS
        IF WINDOWCLOSED() <> 0 THEN g_quit = 1
        IF g_gfx = 0 THEN
            CONTROL GET "lstClienti", "value" INTO g_sel
            IF g_sel <> g_loaded AND g_sel >= 0 AND g_sel < g_count THEN
                CALL LoadRow(g_sel)
            END IF
            SLEEP 10
        END IF
    LOOP
END SUB

' ---------------------------------------------------------------------------
' --selftest: ciclo CRUD completo su un DBF temporaneo, senza UI.
' ---------------------------------------------------------------------------
SUB RunSelfTest()
    DIM fails AS INTEGER, ok AS INTEGER
    DIM n AS INTEGER
    fails = 0
    ok = 0

    g_seed = 0                       ' il selftest parte da un archivio VUOTO
    g_dbPath = g_binDir + "/selftest_clienti.dbf"
    IF IO.FILEEXISTS(g_dbPath) THEN KILL g_dbPath
    IF IO.FILEEXISTS(g_dbPath + ".dbt") THEN KILL g_dbPath + ".dbt"
    IF IO.FILEEXISTS(g_dbPath + ".fpt") THEN KILL g_dbPath + ".fpt"
    IF IO.FILEEXISTS(g_dbPath + ".wal") THEN KILL g_dbPath + ".wal"
    IF IO.FILEEXISTS(g_dbPath + ".wal.xlock") THEN KILL g_dbPath + ".wal.xlock"

    DataOpen()
    PRINT "SELFTEST DB="; g_dbPath

    ' Aggiungi due clienti
    g_cognome = "Rossi" : g_nome = "Mario" : g_sesso = 1 : g_paeseIdx = 0
    g_dn = 15 : g_mn = 3 : g_yn = 1980
    CALL AppendCurrent()
    g_cognome = "Bianchi" : g_nome = "Anna" : g_sesso = 0 : g_paeseIdx = 1
    g_dn = 2 : g_mn = 11 : g_yn = 1992
    CALL AppendCurrent()

    n = CountRecs()
    IF n = 2 THEN ok = ok + 1 ELSE fails = fails + 1 : PRINT "FAIL count after add: "; n

    ' Leggi la prima riga
    g_cognome = "" : g_nome = "" : g_sesso = 0 : g_paeseIdx = 0
    CALL LoadRow(0)
    IF g_cognome = "Rossi" AND g_nome = "Mario" AND g_sesso = 1 AND g_paeseIdx = 0 AND g_dn = 15 AND g_mn = 3 AND g_yn = 1980 THEN
        ok = ok + 1
    ELSE
        fails = fails + 1
        PRINT "FAIL load row 0: "; g_cognome; " "; g_nome; " "; DateText(); " sesso="; g_sesso; " paese="; g_paeseIdx
    END IF

    ' Modifica la prima riga
    g_nome = "Maria" : g_paeseIdx = 2
    CALL SaveRow(0)
    CALL LoadRow(0)
    IF g_nome = "Maria" AND g_paeseIdx = 2 THEN
        ok = ok + 1
    ELSE
        fails = fails + 1
        PRINT "FAIL modify row 0: "; g_nome; " paese="; g_paeseIdx
    END IF

    ' La seconda riga è intatta
    CALL LoadRow(1)
    IF g_cognome = "Bianchi" AND g_nome = "Anna" AND g_dn = 2 AND g_mn = 11 AND g_yn = 1992 THEN
        ok = ok + 1
    ELSE
        fails = fails + 1
        PRINT "FAIL row 1 after modify: "; g_cognome; " "; g_nome; " "; DateText()
    END IF

    ' Elimina la prima riga
    CALL DeleteRow(0)
    n = CountRecs()
    IF n = 1 THEN
        ok = ok + 1
    ELSE
        fails = fails + 1
        PRINT "FAIL count after delete: "; n
    END IF
    CALL LoadRow(0)
    IF g_cognome = "Bianchi" THEN ok = ok + 1 ELSE fails = fails + 1 : PRINT "FAIL row after delete: "; g_cognome

    DBF CLOSE
    IF IO.FILEEXISTS(g_dbPath) THEN KILL g_dbPath
    IF IO.FILEEXISTS(g_dbPath + ".fpt") THEN KILL g_dbPath + ".fpt"
    IF IO.FILEEXISTS(g_dbPath + ".wal") THEN KILL g_dbPath + ".wal"
    IF IO.FILEEXISTS(g_dbPath + ".wal.xlock") THEN KILL g_dbPath + ".wal.xlock"

    PRINT "SELFTEST CHECKS OK="; ok; " FAIL="; fails
    IF fails = 0 THEN
        PRINT "SELFTEST=PASS"
    ELSE
        PRINT "SELFTEST=FAIL"
    END IF
END SUB
