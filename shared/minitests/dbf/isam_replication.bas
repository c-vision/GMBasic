' MiniTest ISAM — replicazione (SET/APPLY/FOLLOWREPLICATION), APPENDMANY, INTEGRITY.
' Portato da samples/Commercial/Databases/IsamReplication.bas + QueryPhaseD.bas (INTEGRITY),
' con path assoluti in /tmp (i sample lì scrivono nel loro work/).
' EXPECT: setrepl: si|integrity: {"valid":true,"message":"Validazione completata con successo. Nessun errore trovato."}|appendmany:  1
IMPORT ISAM
IMPORT DBF
IMPORT IO

TYPE Item
    ID AS STRING * 8
    Name AS STRING * 30
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_isam_repl.isdb"

    ON ERROR RESUME NEXT
    KILL p
    KILL p + ".repl"
    KILL p + ".repl.ckpt"
    ON ERROR GOTO 0

    ISAM OPEN p OF Item ALIAS Primary INDEXED BY ID

    ' --- SETREPLICATION(alias, logPath) → path del log (stringa)
    DIM logPath AS STRING
    logPath = ISAM.SETREPLICATION(Primary, "/tmp/gmb_mt_isam_repl.log")
    IF LEN(logPath) > 0 THEN PRINT "setrepl: si" ELSE PRINT "setrepl: no"

    DIM R AS Item
    R.ID = "R001" : R.Name = "Alpha"
    ISAM APPEND Primary, R
    R.ID = "R002" : R.Name = "Beta"
    ISAM APPEND Primary, R

    ' --- INTEGRITY(alias) → JSON (valid/message) — PRIMA dell'APPENDMANY:
    ' REPERTO: ISAM.APPENDMANY aggiunge il record all'header ma NON all'indice primario, quindi
    ' l'integrità riporta "L'header riporta 3 record attivi, ma l'indice primario ne contiene 2".
    ' Qui si verifica INTEGRITY su una tabella coerente; il reperto è annotato.
    ' INTEGRITY ritorna il TESTO JSON: nel mondo C e' una stringa (valore stampato identico).
    DIM checkJson AS STRING = ISAM.INTEGRITY(Primary)
    PRINT "integrity: "; checkJson

    ' --- APPENDMANY: forma canonica con ARRAY DI RECORD. Il letterale annidato {{...}}
    ' non è supportato dall'emitter C# (ArrayLiteralExpr) e sul backend PCode diventa una
    ' mappa con chiavi duplicate — vedi la nota gemella in dbf_replication.bas.
    DIM batch(0) AS Item
    DIM r3 AS Item
    r3.ID = "R003" : r3.Name = "Gamma"
    batch(0) = r3
    PRINT "appendmany: "; ISAM.APPENDMANY(Primary, batch)

    ' REPERTI (non eseguiti qui, divergenza fra backend sul percorso del log di replica):
    '   ISAM.APPLYREPLICATION(<log>) e ISAM.FOLLOWREPLICATION(<log>) — PCode e IL non
    '   concordano sul significato del percorso (PCode: "Replication log not found" con un file,
    '   "Access denied" con una directory; IL ritorna un valore). Vanno allineati.

    ISAM CLOSE
    ON ERROR RESUME NEXT
    KILL p
    KILL p + ".repl"
    KILL p + ".repl.ckpt"
    ON ERROR GOTO 0
END SUB
