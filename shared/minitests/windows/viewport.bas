' MiniTest WINDOWS — VIEWPORT (comando a spazio del namespace WINDOWS; richiede IMPORT WINDOWS).
' Sintassi: VIEWPORT [(x1, y1)-(x2, y2)]  —  VIEWPORT senza argomenti = reset (0,0)-(0,0).
' Il comando è polimorfico (console/finestra): headless l'oracolo è il wiring
' parse -> codegen -> runtime senza errori su tutte le gambe; la resa pixel richiede
' un display (limite documentato, coperto dai sample windows/ e Clipper/).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: viewport=ok
IMPORT WINDOWS

SUB Main()
    VIEWPORT (1, 1)-(80, 25)
    VIEWPORT (2, 2)-(40, 10)
    VIEWPORT
    PRINT "viewport=ok"
END SUB
