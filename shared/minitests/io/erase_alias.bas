' MiniTest IO — ERASE come alias di KILL (cancellazione file).
' ERASE e' un COMANDO legacy (§2.0 AGENTS.md: niente parentesi), registrato in
' StdLib.Io.cs (TokenKind.Erase → Parser.ParseKillStatement, la stessa di KILL).
' Path ASSOLUTO (/tmp): la CWD differisce tra i backend. Asserzioni booleane (si/no),
' mai valori dipendenti dall'ambiente: la parità fra backend deve restare verde.
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: prima: si|dopo: no
SUB Main()
    DIM path AS STRING
    path = "/tmp/gmb_mt_erase.txt"

    OPEN path FOR WRITE AS #1
    WRITE #1, "x"
    CLOSE #1

    IF IO.FILEEXISTS(path) THEN PRINT "prima: si" ELSE PRINT "prima: no"
    ERASE path
    IF IO.FILEEXISTS(path) THEN PRINT "dopo: si" ELSE PRINT "dopo: no"
END SUB
