' MiniTest IO — IO.FREEFILE(): deve ritornare il PRIMO NUMERO DI FILE LIBERO.
' Era uno stub costante (`1`) in C99 C#, C99Native e VM/C: con più file aperti si otteneva
' sempre 1, cioè un numero già occupato. Ora tutti chiamano gmb_file_free_file() (§0bis).
' Il valore è deterministico (1 → 2 → 3) perché la numerazione parte da 1 e i file aperti
' in questo sample sono consecutivi: è quindi confrontabile fra i backend.
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: ff0:  1|ff1:  2|ff2:  3|ff3:  1
IMPORT IO

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_freefile.txt"

    ' nessun file aperto → il primo libero è 1
    PRINT "ff0: "; IO.FREEFILE()

    DIM a AS LONG
    a = IO.FCREATE(p)
    PRINT "ff1: "; IO.FREEFILE()

    DIM b AS LONG
    b = IO.FOPEN(p)
    PRINT "ff2: "; IO.FREEFILE()

    IO.FCLOSE(a)
    IO.FCLOSE(b)

    ' chiusi tutti → torna a 1 (primo slot libero)
    PRINT "ff3: "; IO.FREEFILE()

    KILL p
END SUB
