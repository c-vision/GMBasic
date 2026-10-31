' MiniTest IO — funzioni che ritornano ARRAY: ReadLines, ReadBytes, DIR.
' Fixture scritta dal sample stesso, con nome unico: contenuto deterministico.
' Path ASSOLUTO (la CWD differisce tra i backend).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: lines:  2|lines0: [uno]|bytes:  8|dir:  1
IMPORT IO

SUB Main()
    DIM path AS STRING
    path = "/tmp/gmb_mt_arrays.txt"

    DIM h AS LONG
    h = IO.FCREATE(path)
    IO.FWRITE(h, "uno" + CHR(10) + "due" + CHR(10))
    IO.FCLOSE(h)

    ' --- ReadLines: 2 righe deterministiche
    DIM lines = IO.READLINES(path)
    PRINT "lines: "; UBOUND(lines) + 1
    PRINT "lines0: ["; lines(0); "]"

    ' --- ReadBytes: 8 byte ("uno"+LF + "due"+LF)
    DIM bytes = IO.READBYTES(path)
    PRINT "bytes: "; UBOUND(bytes) + 1

    ' --- DIR: maschera esatta del file appena creato -> 1 match
    DIM files = IO.DIR(path)
    PRINT "dir: "; UBOUND(files) + 1

    KILL path
END SUB
