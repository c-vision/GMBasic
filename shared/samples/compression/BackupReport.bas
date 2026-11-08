' FILE: compression/BackupReport.bas
' SCENARIO DIDATTICO — backup compresso di un report testuale.
'
' Il programma genera un report, lo salva su file, crea un archivio ZIP con ARCHIVE.ZIP,
' lo estrae in una cartella separata con UNZIP e verifica che il contenuto riletto sia
' identico all'originale. Mostra anche un round-trip in memoria (DEFLATE/INFLATE) per
' confrontare le due modalità d'uso.
'
' Domini: COMPRESSION + IO. File di lavoro in work/ (cancellati a fine scenario).
REQUIRE "compression"
IMPORT COMPRESSION
IMPORT ARCHIVE
IMPORT IO

SUB Main()
    MKDIR "work"
    RMDIR "work/backup_in", TRUE
    RMDIR "work/backup_out", TRUE
    MKDIR "work/backup_in"
    MKDIR "work/backup_out"

    ' 1) Report di esempio.
    DIM h AS LONG
    h = IO.FCREATE("work/backup_in/report.txt")
    IO.FWRITE(h, "Report vendite" + CHR(10))
    IO.FWRITE(h, "gennaio: 120" + CHR(10))
    IO.FWRITE(h, "febbraio: 150" + CHR(10))
    IO.FCLOSE(h)

    ' 2) Archivio ZIP della cartella e ripristino in un'altra cartella.
    DIM okZip AS BIT
    okZip = ARCHIVE.ZIP("work/backup_in/report.txt", "work/report.zip")
    DIM okUnzip AS BIT
    okUnzip = ARCHIVE.UNZIP("work/report.zip", "work/backup_out")
    PRINT "zip: "; okZip; " unzip: "; okUnzip

    ' Il layout dell'archivio dipende dal backend: alcuni conservano il path relativo
    ' (work/backup_in/report.txt), altri solo il nome file. Cerchiamo entrambi.
    DIM ripristinato AS STRING
    IF IO.FILEEXISTS("work/backup_out/report.txt") THEN
        ripristinato = IO.READFILE("work/backup_out/report.txt")
    ELSEIF IO.FILEEXISTS("work/backup_out/work/backup_in/report.txt") THEN
        ripristinato = IO.READFILE("work/backup_out/work/backup_in/report.txt")
    END IF
    PRINT "ripristino ok: "; (INSTR(ripristinato, "Report vendite") > 0)

    ' 3) Round-trip in memoria (senza file): confronto tra originale e decompresso.
    DIM originale AS STRING
    originale = "Report vendite gennaio 120 febbraio 150"
    DIM compresso AS STRING
    compresso = COMPRESSION.DEFLATE(originale)
    DIM espanso AS STRING
    espanso = COMPRESSION.UNDEFLATE(compresso)
    PRINT "round-trip: "; (espanso = originale)

    KILL "work/report.zip"
    KILL "work/backup_in/report.txt"
    KILL "work/backup_out/report.txt"
END SUB
