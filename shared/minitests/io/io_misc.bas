' MiniTest IO — ultimi due simboli senza sample: SEEK (file legacy) e GetFolderPath.
' SEEK lavora sul NUMERO di file legacy (#1), non sull'handle di FOpen: serve OPEN/FOR.
' Path ASSOLUTO (la CWD differisce tra i backend).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: seek_inizio:  0|dopo_seek3:  2|getfolderpath: si|cwd: si
IMPORT IO

SUB Main()
    DIM path AS STRING
    path = "/tmp/gmb_mt_misc.txt"

    OPEN path FOR WRITE AS #1
    WRITE #1, "abcdef"
    CLOSE #1

    ' --- SEEK: posizione corrente del cursore (file legacy aperto in lettura).
    OPEN path FOR INPUT AS #1
    PRINT "seek_inizio: "; IO.SEEK(1)
    ' COMANDO legacy `SEEK n, pos` (posizione 1-based): deve spostare il cursore.
    SEEK 1, 3
    PRINT "dopo_seek3: "; IO.SEEK(1)
    CLOSE #1

    ' --- GetFolderPath: solo un fatto, mai il path grezzo (dipende dall'ambiente)
    IF LEN(IO.GETFOLDERPATH(".")) > 0 THEN PRINT "getfolderpath: si" ELSE PRINT "getfolderpath: no"

    ' --- CWD: la directory corrente. Si asserisce solo il FATTO (non vuota), non il path
    ' (dipende da dove gira il processo e differisce fra le gambe).
    IF LEN(IO.CWD()) > 0 THEN PRINT "cwd: si" ELSE PRINT "cwd: no"

    KILL path
END SUB
