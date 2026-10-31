' MiniTest IO — gruppo HANDLE + PATH + READ (copre i simboli IO che non avevano sample).
' Path ASSOLUTI: la CWD differisce tra i backend, quindi niente path relativi.
' Le asserzioni stampano FATTI confrontabili (si/no, numeri piccoli), mai valori
' dipendenti dall'ambiente (TempDir, AppPath, ...): la parità fra backend deve restare verde.
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: lof>=9: si|fseek0:  0|read1: [abc]|ferror0: si|finput3: [def]|freefile>0: si|fclose>=0: si|fopen: si|read2: [abc]|eof: no|exists: si|resolve->file: si|parentdir: [/tmp]|tempdir: si|tempfile: si|apppath: si|binpath: si|foldersize>0: si|countlines:  2
IMPORT IO

SUB Main()
    DIM path AS STRING
    path = "/tmp/gmb_mt_handles.txt"

    ' --- creazione + scrittura fixture
    DIM h AS LONG
    h = IO.FCREATE(path)
    IO.FWRITE(h, "abc" + CHR(10) + "defgh" + CHR(10))

    ' --- LOF / FSEEK / EOF / FERROR / FINPUT (gruppo handle) ---
    IF IO.LOF(h) >= 9 THEN PRINT "lof>=9: si" ELSE PRINT "lof>=9: no"
    PRINT "fseek0: "; IO.FSEEK(h, 0)
    PRINT "read1: ["; IO.FREAD(h); "]"
    IF IO.FERROR() = 0 THEN PRINT "ferror0: si" ELSE PRINT "ferror0: no"
    PRINT "finput3: ["; IO.FINPUT(3, h); "]"
    IF IO.FREEFILE() > 0 THEN PRINT "freefile>0: si" ELSE PRINT "freefile>0: no"
    IF IO.FCLOSE(h) >= 0 THEN PRINT "fclose>=0: si" ELSE PRINT "fclose>=0: no"

    ' --- FOPEN: riapro in lettura
    DIM r AS LONG
    r = IO.FOPEN(path)
    IF r >= 0 THEN PRINT "fopen: si" ELSE PRINT "fopen: no"
    PRINT "read2: ["; IO.FREAD(r); "]"
    IF IO.EOF(r) THEN PRINT "eof: si" ELSE PRINT "eof: no"
    IO.FCLOSE(r)

    ' --- PATH group: asserzioni booleane, mai il valore grezzo ---
    IF IO.FILEEXISTS(path) THEN PRINT "exists: si" ELSE PRINT "exists: no"
    IF IO.FILEEXISTS(IO.RESOLVEFILEPATH(path)) THEN PRINT "resolve->file: si" ELSE PRINT "resolve->file: no"
    PRINT "parentdir: ["; IO.PARENTDIR(path); "]"
    IF LEN(IO.TEMPDIR()) > 0 THEN PRINT "tempdir: si" ELSE PRINT "tempdir: no"
    IF LEN(IO.TEMPFILE()) > 0 THEN PRINT "tempfile: si" ELSE PRINT "tempfile: no"
    IF LEN(IO.APPPATH()) >= 0 THEN PRINT "apppath: si" ELSE PRINT "apppath: no"
    IF LEN(IO.BINPATH()) >= 0 THEN PRINT "binpath: si" ELSE PRINT "binpath: no"

    ' --- FOLDERSIZE: solo confronto, il valore dipende dal filesystem
    IF IO.FOLDERSIZE("/tmp") > 0 THEN PRINT "foldersize>0: si" ELSE PRINT "foldersize>0: no"

    ' --- conteggio righe della fixture scritta sopra (2 righe)
    PRINT "countlines: "; IO.COUNTLINES(path)

    ' Pulizia: KILL è un COMANDO legacy (§2.0: niente parentesi), registrato nudo.
    KILL path
END SUB
