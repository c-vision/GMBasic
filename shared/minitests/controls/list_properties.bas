' EXPECT: list-props-ok
' ListBox/ComboBox: List, ListIndex, ListCount e Items via CONTROL SET/GET.
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM sel AS INTEGER
  DIM h AS INTEGER
  DIM n AS INTEGER
  DIM s AS STRING
  LISTBOX 2, 2, 20, 5, "A|B|C" INTO sel ID h NAME "L1"
  CONTROL SET "L1", "items", 0, "X|Y|Z"
  CONTROL GET "L1", "listcount" INTO n
  CONTROL GETS "L1", "items" INTO s
  CONTROL SET "L1", "listindex", 1
  CONTROL GET "L1", "listindex" INTO sel
  IF n = 3 AND s = "X|Y|Z" AND sel = 1 THEN
    PRINT "list-props-ok"
  ELSE
    PRINT "list-props-no n="; n; " s="; s; " sel="; sel
  END IF
END SUB
