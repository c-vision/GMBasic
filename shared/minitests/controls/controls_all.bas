' MiniTest NAMESPACE CONTROLS — tutte le 11 funzioni della superficie.
' Predicati di evento del toolkit (headless: nessun evento → 0) e getter di proprietà
' su un controllo inesistente (0/""). Il test verifica dispatch e arità su 4 gambe.
' EXPECT: changed= 0|clicked= 0|dbl= 0|got= 0|lost= 0|key= 0|mc= 0|mdc= 0|mp= 0|prop= 0|props=[]
IMPORT CONTROLS

SUB Main()
  PRINT "changed="; CONTROLS.CHANGED()
  PRINT "clicked="; CONTROLS.CLICKED()
  PRINT "dbl="; CONTROLS.DOUBLECLICKED()
  PRINT "got="; CONTROLS.GOTFOCUSED()
  PRINT "lost="; CONTROLS.LOSTFOCUSED()
  PRINT "key="; CONTROLS.KEYPRESSED()
  PRINT "mc="; CONTROLS.MOUSECLICKED()
  PRINT "mdc="; CONTROLS.MOUSEDBLCLICKED()
  PRINT "mp="; CONTROLS.MOUSEPRESSED()
  PRINT "prop="; CONTROLS.CTRLGETPROP("nope", "text")
  PRINT "props=["; CONTROLS.CTRLGETPROPS("nope", "text"); "]"
END SUB
