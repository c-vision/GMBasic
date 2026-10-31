' MiniTest console — CAPTION (titolo console/finestra).
' Allineamento 4 backend: nessun byte su stdout oltre l'output dichiarato
' (il runtime C scrive l'escape OSC solo su tty; IL/PCode non scrivono nulla).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: after
CAPTION "test-title"
PRINT "after"
