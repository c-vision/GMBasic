' FILE: Computer/ComputerScreenRecord.bas
' =================================================================================================
' COMPUTER.SCREENRECORD(path$, fps) — registra schermo in file video (ffmpeg su macOS)
' Richiede permessi Screen Recording.
' =================================================================================================
IMPORT COMPUTER

SUB Main()
    CLS
    PRINT "=== COMPUTER.SCREENRECORD ==="
    DIM path$ = "work/recording.mp4"
    DIM result$ = COMPUTER.SCREENRECORD(path$, 10)
    PRINT "Result: "; LEFT$(result$, 200)
    IF FILEEXISTS(path$) THEN PRINT "File creato: OK"
    PRINT "=== OK ==="
END SUB