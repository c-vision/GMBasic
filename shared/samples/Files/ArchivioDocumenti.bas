' FILE: Files/ArchivioDocumenti.bas
' SCENARIO DIDATTICO — archiviazione di documenti con IO.
'
' Un ufficio riceve i documenti in una cartella di lavoro e li copia in un archivio
' ricorsivo; poi verifica l'apertura di un documento, legge un registro di righe e
' controlla lo stato dell'ultimo errore I/O.
'
' Domini: IO (XCOPY, FOPEN, FERROR, READLINES) + base. File in work/ (rimossi a fine run).
IMPORT IO

SUB Main()
    MKDIR "work"
    RMDIR "work/documenti", TRUE
    RMDIR "work/archivio", TRUE
    MKDIR "work/documenti"
    MKDIR "work/documenti/allegati"

    ' 1) Documenti in arrivo.
    DIM h AS LONG
    h = IO.FCREATE("work/documenti/contratto.txt")
    IO.FWRITE(h, "Contratto 2026" + CHR(10))
    IO.FCLOSE(h)
    h = IO.FCREATE("work/documenti/allegati/tavola.txt")
    IO.FWRITE(h, "Tavola allegata" + CHR(10))
    IO.FCLOSE(h)
    PRINT "documenti creati: "; IO.FILEEXISTS("work/documenti/contratto.txt"); _
          " "; IO.FILEEXISTS("work/documenti/allegati/tavola.txt")

    ' 2) Copia ricorsiva nell'archivio.
    XCOPY "work/documenti" TO "work/archivio", TRUE
    PRINT "archivio: "; IO.FILEEXISTS("work/archivio/contratto.txt"); _
          " "; IO.FILEEXISTS("work/archivio/allegati/tavola.txt")

    ' 3) Apertura di un documento (l'handle è un numero interno: si verifica > 0).
    DIM f AS LONG
    f = IO.FOPEN("work/archivio/contratto.txt")
    PRINT "apertura ok: "; (f > 0)
    IF f > 0 THEN IO.FCLOSE(f)

    ' 4) Registro delle operazioni lette in blocco.
    h = IO.FCREATE("work/registro.log")
    IO.FWRITE(h, "09:00 arrivo documenti" + CHR(10))
    IO.FWRITE(h, "09:05 copia archivio" + CHR(10))
    IO.FWRITE(h, "09:10 verifica" + CHR(10))
    IO.FCLOSE(h)
    DIM righe = IO.READLINES("work/registro.log")
    PRINT "righe registro: "; UBOUND(righe) + 1
    PRINT "ultima: "; righe(UBOUND(righe))

    ' 5) Stato dell'ultimo errore I/O (0 = nessun errore).
    PRINT "nessun errore: "; (IO.FERROR() = 0)

    RMDIR "work/documenti", TRUE
    RMDIR "work/archivio", TRUE
    KILL "work/registro.log"
END SUB
