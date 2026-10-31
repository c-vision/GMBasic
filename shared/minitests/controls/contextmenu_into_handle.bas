' Minitest CONTEXTMENU — `INTO` assegna l'HANDLE del controllo (non la lista items), 4 backend.
' EXPECT: contextmenu-handle-ok
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM cm AS INTEGER
  CONTEXTMENU 2, 13, 20, 6, ["Apri", "Salva", "--", "Esporta"] INTO cm
  DIM cm2 AS INTEGER
  CONTEXTMENU 2, 20, 20, 6, ["X", "Y"] INTO cm2
  ' handle: >= 0 e consecutivo tra due controlli creati di seguito
  IF cm >= 0 AND cm2 = cm + 1 THEN
    PRINT "contextmenu-handle-ok"
  ELSE
    PRINT "contextmenu-handle-no " + STR(cm) + " " + STR(cm2)
  END IF
END SUB
