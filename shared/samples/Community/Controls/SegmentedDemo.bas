' SegmentedDemo.bas — demo d'applicazione del controllo `Segmented`
' (definito in Controls/Segmented.bas, importato via NAMESPACE). Esempio didattico.
IMPORT CustomControls

SUB Main()
  DIM s AS Segmented = NEW Segmented()
  CONTROL ADD s AT 2, 2 SIZE 40, 3
  s.SetOrigin(2, 2, 40, 3)
  s.Configure(0)
  s.AddItem("1m") : s.AddItem("5m") : s.AddItem("15m") : s.AddItem("1h")
  s.SetSelected(0)

  PRINT "n="; s.Count(); " sel="; s.SelectedText()
  s.Hit(25, 1)
  PRINT "dopo click sel="; s.SelectedText()
  PRINT "OK-segmented-uc"
END SUB
