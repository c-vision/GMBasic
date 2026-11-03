' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Files/FilesFullCoverage.bas
' Copertura completa delle funzioni Files/IO non ancora coperte.
IMPORT IO

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "Files - Copertura Completa"
    PRINT "============================================================"
    PRINT

    ' --- Path helpers (deterministici: proprietà, non lunghezze assolute) ---
    DIM seq AS STRING
    seq = g_work + "/sequential.txt"
    DIM hs AS LONG
    hs = IO.FCREATE(seq)
    IO.FWRITE(hs, "riga uno" + CHR$(10) + "riga due" + CHR$(10))
    IO.FCLOSE(hs)

    PRINT "Cwd len: "; LEN(IO.Cwd()) > 0
    PRINT "FileExists: "; IO.FileExists(seq)
    PRINT "DirExists: "; IO.DirExists(".")
    PRINT "GetFolderPath: "; IO.DirExists(IO.GetFolderPath("."))
    PRINT "NormalizePath: "; IO.DirExists(IO.NormalizePath(g_work))
    PRINT "FolderSize: "; IO.FolderSize(".") >= 0
    PRINT

    ' --- FCREATE / FWRITE / FREAD / FSEEK / FCLOSE ---
    PRINT "-- FCREATE / FWRITE / FREAD --"
    DIM h = IO.FCreate(g_work + "/coverage_test.txt")
    PRINT "FCreate h: "; h
    IO.FWrite(h, "Ciao mondo")
    IO.FSeek(h, 0)
    DIM s AS STRING
    ' FREAD come COMANDO (B): legge e SCRIVE nel buffer. La forma FUNZIONE `IO.FREAD(h)`
    ' ritorna invece i dati letti e non scrive nulla (AGENTS §1/§3).
    FREAD h INTO s
    PRINT "FRead: "; s
    IO.FSeek(h, 0)
    PRINT "FRead (funzione): "; IO.FREAD(h)
    IO.FClose(h)
    PRINT

    ' --- Path helpers aggiuntivi ---
    PRINT "-- AppPath / TempFile / TempDir --"
    PRINT "AppPath: "; LEN(IO.AppPath()) > 0
    PRINT "TempFile: "; LEN(IO.TempFile()) > 0
    PRINT "TempDir: "; LEN(IO.TempDir()) > 0
    PRINT "BinPath: "; LEN(IO.BinPath()) > 0
    PRINT "ParentDir: "; IO.DirExists(IO.ParentDir(seq))
    PRINT "ResolveFilePath: "; IO.FileExists(IO.ResolveFilePath(seq))
    PRINT

    ' --- READBYTES / READLINES / READFILE / COUNTLINES ---
    PRINT "-- READ* --"
    DIM bytes() = IO.ReadBytes(seq)
    PRINT "ReadBytes len: "; LEN(bytes())
    DIM lines() = IO.ReadLines(seq)
    PRINT "ReadLines len: "; LEN(lines())
    DIM whole AS STRING
    whole = IO.ReadFile(seq)
    PRINT "ReadFile len: "; LEN(whole)
    PRINT "ReadFile non-empty: "; LEN(whole) > 0
    PRINT "CountLines: "; IO.CountLines(seq)
    PRINT

    ' --- KILL / ERASE (comandi legacy §2.0, ERASE alias di KILL) ---
    PRINT "-- KILL / ERASE --"
    DIM killme AS STRING
    killme = g_work + "/coverage_killme.txt"
    DIM hk AS LONG
    hk = IO.FCREATE(killme)
    IO.FWRITE(hk, "da cancellare")
    IO.FCLOSE(hk)
    IF IO.FILEEXISTS(killme) THEN PRINT "kill-prima: si" ELSE PRINT "kill-prima: no"
    KILL killme
    IF IO.FILEEXISTS(killme) THEN PRINT "kill-dopo: si" ELSE PRINT "kill-dopo: no"
    DIM eraseme AS STRING
    eraseme = g_work + "/coverage_eraseme.txt"
    DIM he AS LONG
    he = IO.FCREATE(eraseme)
    IO.FWRITE(he, "da cancellare")
    IO.FCLOSE(he)
    IF IO.FILEEXISTS(eraseme) THEN PRINT "erase-prima: si" ELSE PRINT "erase-prima: no"
    ERASE eraseme
    IF IO.FILEEXISTS(eraseme) THEN PRINT "erase-dopo: si" ELSE PRINT "erase-dopo: no"
    PRINT

    PRINT "Test Completato."
END SUB
