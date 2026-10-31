' MiniTest IO — IO.READFILE(path): intero file come stringa.
' Semantica (identica sulle 4 gambe): file assente/vuoto → "" ; contenuto NON alterato
' (i terminatori di riga restano); BOM UTF-8 iniziale rimosso (come File.ReadAllText lato IL,
' gmb_read_file_text lato C).
' Fixture scritta dal test stesso su path ASSOLUTO (la CWD differisce fra i backend).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: eq: 1|tlen:  8|missing-len:  0|lines:  2|bytes:  8
IMPORT IO

SUB Main()
    DIM path AS STRING
    path = "/tmp/gmb_mt_readfile.txt"

    DIM h AS LONG
    h = IO.FCREATE(path)
    IO.FWRITE(h, "uno" + CHR(10) + "due" + CHR(10))
    IO.FCLOSE(h)

    ' --- READFILE: contenuto integrale, terminatori di riga inclusi
    DIM text AS STRING
    text = IO.READFILE(path)
    IF text = "uno" + CHR(10) + "due" + CHR(10) THEN
        PRINT "eq: 1"
    ELSE
        PRINT "eq: 0"
    END IF
    PRINT "tlen: "; LEN(text)

    ' --- File assente → stringa vuota (IlRuntime.ReadFile: File.Exists falso; C: fopen NULL)
    PRINT "missing-len: "; LEN(IO.READFILE("/tmp/gmb_mt_readfile_absent_9f3.txt"))

    ' --- Cross-check con le altre letture in blocco sullo stesso file
    DIM lines = IO.READLINES(path)
    PRINT "lines: "; UBOUND(lines) + 1
    DIM bytes = IO.READBYTES(path)
    PRINT "bytes: "; UBOUND(bytes) + 1

    KILL path
END SUB
