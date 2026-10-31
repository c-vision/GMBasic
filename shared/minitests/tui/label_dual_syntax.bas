' MiniTest LABEL — doppia sintassi: VELOCE (posizionale) e VERBOSA (AT/SIZE/TEXT).
IMPORT CONTROLS

SUB Main()
  LABEL 1, 2, 20, "veloce"                        ' sintassi veloce
  LABEL AT 3, 2 WIDTH 20 TEXT "verbosa"           ' sintassi verbosa
  PRINT "OK-label-dual"
END SUB
