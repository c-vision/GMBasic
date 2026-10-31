' Minitest ContextMenu UserControl (§12.4) — popup + navigazione + scelta, 4 backend.
' EXPECT: contextmenu-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM m AS ContextMenu = NEW ContextMenu()
  CONTROL ADD m AT 1, 1 SIZE 24, 12
  m.Configure(18)
  m.SetItem("New")
  m.SetItem("Open")
  m.SetSeparator("---")
  m.SetItem("Export")
  m.SetItem("Delete")
  m.SetEnabled(4, 0)
  DIM o0 AS INTEGER
  o0 = m.IsOpen()
  m.Popup(2, 2)
  DIM o1 AS INTEGER
  o1 = m.IsOpen()
  m.MoveSel(1)
  DIM s1 AS INTEGER
  s1 = m.Sel()
  m.MoveSel(1)
  DIM s2 AS INTEGER
  s2 = m.Sel()
  m.HandleKey(13)
  DIM ch AS INTEGER
  ch = m.Chosen()
  m.Popup(0, 0)
  m.MousePress(3, 1)
  DIM cl AS STRING
  cl = m.ChosenText()
  CONTROL REMOVE m
  IF o0 = 0 AND o1 = 1 AND s1 = 1 AND s2 = 3 AND ch = 3 AND cl = "Open" THEN
    PRINT "contextmenu-ok"
  ELSE
    PRINT "contextmenu-no "; o0; "|"; o1; "|"; s1; "|"; s2; "|"; ch; "|"; cl
  END IF
END SUB
