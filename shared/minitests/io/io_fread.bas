' MiniTest IO — FREAD: (A) FUNZIONE PURA `FREAD(handle [, count])` che ritorna i dati letti,
'                 (B) COMANDO `FREAD handle [, count] INTO buffer` che scrive nel buffer.
' AGENTS §1/§3: le FUNZIONI non scrivono mai, i COMANDI non ritornano valori (§2: il comando
' usa la sintassi legacy a spazio, senza parentesi).
' Path ASSOLUTO (la CWD differisce tra i backend).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: abc|def|[abc]|[abc]|0
IMPORT IO

SUB Main()
    DIM h = IO.FCREATE("/tmp/gmb_mt_fread.txt")
    IO.FWRITE(h, "abc" + CHR(10) + "defgh" + CHR(10))

    ' (A) funzione pura: il valore ritornato è il dato letto, nessun buffer coinvolto
    IO.FSEEK(h, 0)
    PRINT IO.FREAD(h)
    PRINT IO.FREAD(h, 3)

    ' (B) comando: legge e SCRIVE nel buffer
    IO.FSEEK(h, 0)
    DIM b1 AS STRING
    FREAD h INTO b1
    PRINT "[" + b1 + "]"

    IO.FSEEK(h, 0)
    DIM b2 AS STRING
    FREAD h, 3 INTO b2
    PRINT "[" + b2 + "]"

    PRINT IO.FCLOSE(h)
END SUB
