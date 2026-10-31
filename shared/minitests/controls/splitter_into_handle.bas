' Minitest SPLITTER — `INTO` assegna l'HANDLE del controllo (parità 4 backend).
' EXPECT: splitter-handle-ok
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM sp AS INTEGER
  SPLITTER 2, 2, 40, 10, 0 INTO sp
  DIM sp2 AS INTEGER
  SPLITTER 2, 20, 40, 10, 1 INTO sp2
  ' handle: >= 0 e consecutivo tra due controlli creati di seguito
  IF sp >= 0 AND sp2 = sp + 1 THEN
    PRINT "splitter-handle-ok"
  ELSE
    PRINT "splitter-handle-no " + STR(sp) + " " + STR(sp2)
  END IF
END SUB
