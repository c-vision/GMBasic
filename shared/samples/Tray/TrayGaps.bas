' FILE: Tray/TrayGaps.bas
' Esercita TUTTE e 10 le funzioni del namespace TRAY in sequenza, con degrado pulito
' quando il tray non è disponibile (headless/CI): TRAYAVAILABLE, TRAYBACKEND,
' TRAYCREATE, TRAYITEM, TRAYSEP, TRAYTOOLTIP, TRAYPOLL, TRAYLAST, TRAYREMOVE, TRAYSELFTEST.

IMPORT TRAY

SUB Main()
    PRINT "TRAYBACKEND()   = "; TRAYBACKEND()
    PRINT "TRAYAVAILABLE() = "; TRAYAVAILABLE()
    PRINT "TRAYSELFTEST(prima) = "; TRAYSELFTEST()

    DIM r AS LONG
    r = TRAYCREATE("", "TrayGaps")
    PRINT "TRAYCREATE      = "; r
    PRINT "TRAYITEM(1)     = "; TRAYITEM(1, "Stato")
    PRINT "TRAYITEM(99)    = "; TRAYITEM(99, "Esci")
    PRINT "TRAYSEP()       = "; TRAYSEP()
    PRINT "TRAYTOOLTIP()   = "; TRAYTOOLTIP("stato: pronto")

    SLEEP 120
    PRINT "TRAYPOLL()      = "; TRAYPOLL()
    PRINT "TRAYLAST()      = "; TRAYLAST()

    PRINT "TRAYREMOVE()    = "; TRAYREMOVE()
    SLEEP 120
    PRINT "TRAYPOLL(2)     = "; TRAYPOLL()
    PRINT "TRAYSELFTEST(dopo) = "; TRAYSELFTEST()

    IF TRAYAVAILABLE() = 0 THEN
        PRINT "nota: tray non disponibile su questo host (atteso su headless)"
    END IF

    PRINT "TrayGaps: completato."
END SUB
