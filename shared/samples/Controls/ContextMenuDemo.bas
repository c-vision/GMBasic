' ContextMenuDemo.bas — demo d'applicazione del controllo (Controls/ContextMenu.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM m AS ContextMenu = NEW ContextMenu()
  CONTROL ADD m AT 1, 1 SIZE 24, 12
  m.Configure(18)
  m.SetItem("Apri")
  m.SetItem("Salva")
  m.SetSeparator("---")
  m.SetItem("Esporta")
  m.SetItem("Elimina")
  m.SetEnabled(4, 0)                    ' "Elimina" disabilitata
  PRINT "items="; m.Count(); " open="; m.IsOpen()
  m.Popup(2, 2)
  PRINT "open="; m.IsOpen(); " sel="; m.Sel()
  m.MoveSel(1)                          ' -> "Salva"
  PRINT "sel="; m.SelText()
  m.MoveSel(1)                          ' salta il separatore e "Elimina" (disabilitata) -> "Esporta"
  PRINT "sel="; m.SelText()
  m.HandleKey(13)                       ' invio -> sceglie
  PRINT "chosen="; m.Chosen(); " label="; m.ChosenText(); " open="; m.IsOpen()
  ' click diretto su una voce
  m.Popup(0, 0)
  m.MousePress(3, 1)                    ' riga 1 -> "Salva"
  PRINT "click="; m.ChosenText()
  CONTROL REMOVE m
  PRINT "contextmenu-ok"
END SUB
