' Minitest §4 Coda — props stringa (CONTROL GETS "items"/"preset"/"barstyle"/"format")
' + PROGRESS showpercent. 4 backend.  EXPECT: coda-props-ok
IMPORT CONTROLS
SCREEN 0
DIM v AS INTEGER

SUB Main()
  ' LISTBOX items
  LISTBOX 2, 2, 20, 4, ["A", "B"] INTO v NAME "lb"
  DIM lb AS STRING
  CONTROL GETS "lb", "items" INTO lb

  ' COMBOBOX items
  COMBOBOX 2, 24, 20, ["IT", "FR", "DE"] INTO v NAME "cb"
  DIM cb AS STRING
  CONTROL GETS "cb", "items" INTO cb

  ' TAB tabs
  TAB 8, 2, 40, 3, ["X", "Y"] INTO v NAME "tb"
  DIM tb AS STRING
  CONTROL GETS "tb", "items" INTO tb

  ' SPINNER preset
  SPINNER 12, 2, "1|2|3" INTO v NAME "sp"
  CONTROL SET "sp", "preset", 0, "10|20|30"
  DIM sp AS STRING
  CONTROL GETS "sp", "preset" INTO sp

  ' PROGRESS: barstyle + showpercent
  PROGRESS 14, 2, 20, 50 INTO v NAME "pg"
  CONTROL SET "pg", "barstyle", 0, "#"
  CONTROL SET "pg", "showpercent", 1, ""
  DIM pgp AS INTEGER
  CONTROL GET "pg", "showpercent" INTO pgp
  DIM pgs AS STRING
  CONTROL GETS "pg", "barstyle" INTO pgs

  ' TEXTBOX format (INTO stringa: il target è il testo legato al campo)
  DIM tftext AS STRING
  TEXTBOX 16, 2, 20, 1 INTO tftext NAME "tf"
  CONTROL SET "tf", "format", 0, "##"
  DIM tfs AS STRING
  CONTROL GETS "tf", "format" INTO tfs

  IF lb = "A|B" AND cb = "IT|FR|DE" AND tb = "X|Y" AND sp = "10|20|30" AND pgp = 1 AND pgs = "#" AND tfs = "##" THEN
    PRINT "coda-props-ok"
  ELSE
    PRINT "coda-props-no lb=" + lb + " cb=" + cb + " tb=" + tb + " sp=" + sp + " pgp=" + STR(pgp) + " pgs=" + pgs + " tfs=" + tfs
  END IF
END SUB
