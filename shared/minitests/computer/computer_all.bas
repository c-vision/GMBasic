' MiniTest NAMESPACE COMPUTER — superficie COMPLETA (12 funzioni), parità di superficie.
' Click/DoubleClick/Type/Key/HotKey/Move e la registrazione schermo NON vengono eseguiti
' (inietterebbero input o avvierebbero registrazioni sulla macchina): sono compilati e
' linkati nella stessa forma sui 4 backend nel ramo IF 0. SCREENRECORDSTATUS è eseguita
' davvero (sola lettura).
' EXPECT: status_len>=0=-1
IMPORT COMPUTER

SUB Main()
  PRINT "status_len>=0="; (LEN(COMPUTER.SCREENRECORDSTATUS()) >= 0)

  IF 0 THEN
    _b1 = COMPUTER.Click(0, 0)
    _b2 = COMPUTER.DoubleClick(0, 0)
    _b3 = COMPUTER.HotKey("")
    _b4 = COMPUTER.Key("")
    _b5 = COMPUTER.Move(0, 0)
    _b6 = COMPUTER.ScreenRecord("", "")
    _b7 = COMPUTER.ScreenRecordStart("", "")
    _b8 = COMPUTER.ScreenRecordStatus()
    _b9 = COMPUTER.ScreenRecordStop()
    _b10 = COMPUTER.Screenshot("")
    _b11 = COMPUTER.Type("")
    _b12 = COMPUTER.Windows()
  END IF
END SUB
