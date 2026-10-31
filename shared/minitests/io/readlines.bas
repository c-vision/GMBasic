' MiniTest IO — funzioni che ritornano ARRAY: ReadLines, ReadBytes.
' Fixture scritta dal sample stesso, con nome unico: contenuto deterministico.
' Path ASSOLUTO (la CWD differisce fra i backend).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: lines:  2|lines0: [uno]|lines1: [due]|bytes:  8
IMPORT IO

SUB Main()
    DIM path AS STRING
    path = "/tmp/gmb_mt_readlines.txt"

    DIM h AS LONG
    h = IO.FCREATE(path)
    IO.FWRITE(h, "uno" + CHR(10) + "due" + CHR(10))
    IO.FCLOSE(h)

    ' --- ReadLines: 2 righe deterministiche, nessun terminatore conservato
    DIM lines = IO.READLINES(path)
    PRINT "lines: "; UBOUND(lines) + 1
    PRINT "lines0: ["; lines(0); "]"
    PRINT "lines1: ["; lines(1); "]"

    ' --- ReadBytes: "uno"+LF + "due"+LF = 8 byte
    DIM bytes = IO.READBYTES(path)
    PRINT "bytes: "; UBOUND(bytes) + 1

    KILL path
END SUB
