' ControlThemeDemo.bas — esempio d'applicazione di `ForeColor`/`BackColor` (proprietà base della
' classe `Control`, §11.1): un "tema" applicato a più controlli — un UserControl LOCALE e un
' controllo della LIBRERIA `CustomControls` (`IMPORT CustomControls`). Dimostra scrittura
' (`istanza.ForeColor = "#RRGGBB"`) e lettura, su 4 backend (IL / PCode / VM-C / C99Native).
IMPORT CONTROLS
IMPORT CustomControls

' Un controllo utente "riquadro" con Paint minimale.
CLASS ThemeBox INHERITS Control
  SUB Paint(g AS ANY)
    g.Clear()
  END SUB
END CLASS

SUB Main()
  ' --- controllo utente locale: tema scuro ---
  DIM tb AS ThemeBox = NEW ThemeBox()
  CONTROL ADD tb AT 2, 2 SIZE 30, 6
  tb.ForeColor = "#E0E0E0"
  tb.BackColor = "#101820"
  PRINT "tb fg="; tb.ForeColor; " bg="; tb.BackColor

  ' --- controllo della libreria CustomControls (Controls/Segmented.bas): tinte diverse ---
  DIM seg AS Segmented = NEW Segmented()
  CONTROL ADD seg AT 2, 10 SIZE 30, 3
  seg.SetOrigin(2, 10, 30, 3)
  seg.Configure(0)
  seg.AddItem("1m") : seg.AddItem("5m") : seg.AddItem("1h")
  seg.SetSelected(1)
  seg.ForeColor = "#FFD166"
  seg.BackColor = "#003049"
  PRINT "seg fg="; seg.ForeColor; " bg="; seg.BackColor; " sel="; seg.SelectedText()

  ' --- indipendenza per istanza ---
  PRINT "tb fg="; tb.ForeColor
  PRINT "theme-ok"
END SUB
