' EXPECT: setrepl: /|appendmany:  2|lastrec:  2|lupdate: -1|applyrepl: si|followrepl: si
' MiniTest DBF — replicazione (SET/APPLY/FOLLOWREPLICATION), APPENDMANY, LUPDATE.
' Portato dal pattern di samples/Databases/{DbfReplication,DbDbfAppendMany}.bas,
' con path ASSOLUTI in /tmp (i sample lì scrivono nello work/ del sample; qui serve
' CWD-indipendenza perché la CWD differisce fra i backend).
IMPORT DBF
IMPORT IO

TYPE TRow
    Name AS STRING * 20
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_db_repl.dbf"

    ON ERROR RESUME NEXT
    KILL p
    KILL p + ".repl"
    KILL p + ".repl.ckpt"
    ON ERROR GOTO 0

    DBF CREATE p FROM TYPE TRow
    DBF USE p ALIAS "Primary" AS TRow

    ' --- SETREPLICATION(logPath) → path del log di replica (stringa)
    DIM logPath AS STRING
    logPath = DBF.SETREPLICATION(Primary, "/tmp/gmb_mt_db_repl.log")
    PRINT "setrepl: "; LEFT(logPath, 1)

    ' --- APPENDMANY(alias, records): inserimento in blocco.
    ' FORMA CANONICA: un ARRAY di record (DIM recs(1) AS TRow). Il letterale annidato
    ' {{..},{..}} NON è equivalente: sul backend PCode il literal diventa una mappa con chiavi
    ' duplicate (una sola voce) → PCode 1, IL 2. La LISTA (`LIST OF Tipo`) non è supportata dal
    ' backend IL (NotSupportedException) — vedi i sample ufficiali per il ramo non coperto.
    DIM recs(1) AS TRow
    ' NB: il batch si costruisce con un record SCALARE poi copiato nell'array. La scrittura
    ' diretta del campo su un ELEMENTO di array boxato (`recs(0).Name = …`) fa uscire
    ' l'emitter C99Native dal ramo che emette la label del gestore ON ERROR
    ' (`__resume_end_N:` mancante → C non compilabile); è un reperto aperto dell'emitter, non
    ' di questo sample. L'output è identico.
    DIM r AS TRow
    r.Name = "Alpha"
    recs(0) = r
    r.Name = "Beta"
    recs(1) = r
    PRINT "appendmany: "; DBF.APPENDMANY(recs)
    PRINT "lastrec: "; DBF.LASTREC()

    ' --- LUPDATE(): data dell'ultimo aggiornamento — STRINGA "YYYY-MM-DD" (il tipo dichiarato
    ' dalla StdLib è una data; PCode la stampa via GbDate, IL e VM/C dal runtime condiviso).
    lu$ = DBF.LUPDATE()
    PRINT "lupdate: "; LEN(lu$) > 0

    ' --- APPLYREPLICATION(logPath) / FOLLOWREPLICATION(logPath): 1 argomento (le varianti
    ' con fileNumber prendono un numero, non l'alias).
    ' APPLY/FOLLOW: si asserisce il FATTO (nessuna eccezione, conteggio non negativo), NON il
    ' conteggio: dipende dal checkpoint di replica, che è condiviso fra le gambe dello stesso
    ' processo (la prima gamba applica le righe, la seconda le trova già applicate → stato, non bug).
    IF DBF.APPLYREPLICATION("/tmp/gmb_mt_db_repl.log") >= 0 THEN PRINT "applyrepl: si" ELSE PRINT "applyrepl: no"
    IF DBF.FOLLOWREPLICATION("/tmp/gmb_mt_db_repl.log") >= 0 THEN PRINT "followrepl: si" ELSE PRINT "followrepl: no"
    DBF CLOSE
    ON ERROR RESUME NEXT
    KILL p
    KILL p + ".repl"
    KILL p + ".repl.ckpt"
    ON ERROR GOTO 0
END SUB
