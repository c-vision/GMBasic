' Minitest TRAY — parità fra i 4 backend (IL/PCodeVM/VMC/C99/C99Native).
' Verifica: backend, disponibilità, create/item/sep/tooltip/poll/last/remove/selftest.
IMPORT TRAY
PRINT "backend="; TRAYBACKEND()
PRINT "available="; TRAYAVAILABLE()
PRINT "selftest0="; TRAYSELFTEST()
DIM r AS LONG
r = TRAYCREATE("", "tray-parity")
PRINT "create="; r
r = TRAYITEM(1, "Stato")
PRINT "item1="; r
r = TRAYSEP()
PRINT "sep="; r
r = TRAYITEM(99, "Esci")
r = TRAYTOOLTIP("tray-parity tooltip")
PRINT "tooltip="; r
DIM i AS LONG
FOR i = 1 TO 10
  SLEEP 50
  r = TRAYPOLL()
NEXT
PRINT "selftest1="; TRAYSELFTEST()
PRINT "last="; TRAYLAST()
TRAYREMOVE()
FOR i = 1 TO 5
  SLEEP 50
  r = TRAYPOLL()
NEXT
PRINT "selftest2="; TRAYSELFTEST()
PRINT "DONE"
