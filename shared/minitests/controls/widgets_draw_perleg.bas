' TAG: interactive
' MiniTest: widget TUI che DISEGNANO il framebuffer su stdout (SPINNER/STATUSBAR/
' TBROWSE) — esclusi dal cross-leg (`IsParityExcluded`) e dal gate con EXPECT statico
' (il render non è confrontabile); verificati per-gamba: EMISSIONE + COMPILAZIONE +
' LINK su IL/PCode/VM-C/C99Native (parità emissione 4 backend).
' EXPECT: wdraw-ok
IMPORT CONTROLS
SUB Main()
  DIM s AS STRING
  SPINNER 1, 4, "a,b" INTO s
  STATUSBAR "stato"
  DIM cols(1) AS STRING
  cols(0) = "Nome"
  cols(1) = "Valore"
  DIM dat(1) AS STRING
  dat(0) = "a"
  dat(1) = "1"
  TBROWSE 1, 1, 30, 5, cols, dat INTO s
  PRINT "wdraw-ok"
END SUB