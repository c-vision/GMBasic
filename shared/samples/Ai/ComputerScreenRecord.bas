' PARITY-EXCLUDE: timing nondeterministico (durationMs varia a ogni run anche tra gambe C#) + registrazione OS reale (2026-09-23, Parita-5-Backend-Residui §2B; shape START/STATUS allineate VM-C nello stesso change)
' FILE: Ai/ComputerScreenRecord.bas
' Screen recorder per agenti (COMPUTER.SCREENRECORD) — Fase 1bis CUA #7 Astra §8bis

IMPORT COMPUTER

SUB Main()
    PRINT "=== Computer ScreenRecord ==="

    DIM path$ AS STRING = "work/rec_test.mp4"
    DIM fps AS DOUBLE = 10

    ' Avvia registrazione (GPM PARK, ffmpeg avfoundation su macOS)
    DIM start$ AS STRING = COMPUTER.SCREENRECORDSTART(path$, fps)
    PRINT "START: "; start$

    ' Simula lavoro agente (click/type)
    SLEEP 2000

    ' Ferma e verifica
    DIM stop$ AS STRING = COMPUTER.SCREENRECORDSTOP()
    PRINT "STOP: "; LEFT(stop$, 300)

    ' Stato
    DIM st$ AS STRING = COMPUTER.SCREENRECORDSTATUS()
    PRINT "STATUS: "; LEFT(st$, 200)

    ' Verifica file
    IF FILEEXISTS(path$) THEN
        PRINT "FILEEXISTS: OK"
    ELSE
        PRINT "FILEEXISTS: FAIL"
    END IF

    PRINT "=== ScreenRecord OK ==="
END SUB
