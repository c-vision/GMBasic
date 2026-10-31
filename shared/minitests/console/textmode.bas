' MiniTest console — TEXTMODE (selezione modalita' console) + costanti %SDL/%C64/%ANSI/%SYSTEM.
' Allineamento 4 backend: verifica i VALORI delle costanti (0/1/2/3) e che TEXTMODE accetti
' sia le costanti sia gli interi, senza output ne' errore. In headless TEXTMODE e' no-op
' (NullGraphicsService / vm_sys_textmode).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso.
' EXPECT: 0|1|2|3|TEXTMODE-OK
PRINT %SDL
PRINT %C64
PRINT %ANSI
PRINT %SYSTEM
TEXTMODE %SDL
TEXTMODE %C64
TEXTMODE %ANSI
TEXTMODE %SYSTEM
TEXTMODE 0
TEXTMODE 1
TEXTMODE 2
TEXTMODE 3
PRINT "TEXTMODE-OK"
