' MiniTest IO/MEMORYMAP — COMANDI di scrittura (MMAPPOKE, MMAPWRITEBYTES)
' (le funzioni MEMORYMAP/MMAPPEEK/MMAPREADBYTES sono in minitests/io/mmap.bas).
' Path relativi sotto bin/ (creato dal test): la CWD e' la cartella del file su
' tutte le gambe. Asserzioni booleane (si/no), mai valori dipendenti dall'ambiente
' (l'handle grezzo differisce per design fra i backend).
' EXPECT: poke=ok|write=ok|partial=ok|close=ok
IMPORT IO
IMPORT MEMORYMAP

SUB Main()
    MKDIR "bin"
    DIM f = IO.FCREATE("bin/mmap_rw.dat")
    IO.FWRITE(f, "hello")
    IO.FCLOSE(f)
    DIM mm = IO.MEMORYMAP("bin/mmap_rw.dat")

    ' --- MMAPPOKE: scrittura di un singolo byte ---
    MMAPPOKE mm, 0, 65
    IF IO.MMAPPEEK(mm, 0) = 65 THEN PRINT "poke=ok" ELSE PRINT "poke=no"

    ' --- MMAPWRITEBYTES: scrittura di un blocco BYTE() ---
    DIM buf(2) AS BYTE
    buf(0) = 10
    buf(1) = 20
    buf(2) = 30
    MMAPWRITEBYTES mm, 0, buf()
    DIM r = IO.MMAPREADBYTES(mm, 0, 3)
    IF r[0] = 10 AND r[1] = 20 AND r[2] = 30 THEN PRINT "write=ok" ELSE PRINT "write=no"

    ' --- MMAPWRITEBYTES con COUNT parziale (solo 2 byte all'offset 5) ---
    DIM p(2) AS BYTE
    p(0) = 100
    p(1) = 101
    p(2) = 102
    MMAPWRITEBYTES mm, 5, p(), 2
    IF IO.MMAPPEEK(mm, 5) = 100 AND IO.MMAPPEEK(mm, 6) = 101 THEN PRINT "partial=ok" ELSE PRINT "partial=no"

    ' --- CLOSE della mappa (comando I/O generico, non IO.CLOSE) ---
    CLOSE mm
    PRINT "close=ok"
END SUB
