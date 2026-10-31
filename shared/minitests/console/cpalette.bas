' MiniTest: CPALETTE numerico nudo (gruppo Console, parità 4 backend).
' CPALETTE idx, r, g, b: entry di palette. Su VM/C era headless_big (push 0.0
' orfano); su C99Native errore esplicito (2026-09-21: vm_sys_palette_339 reale via
' gmb_gfx_palette + st_cpalette). Le sottoforme USE/DEFINE/REPLACE/SAVE/LOAD
' restano C#-only (PaletteManager): residuo tracciato, non parte di questo oracolo.
' Nessun output: l'oracolo è il marker finale.
' EXPECT: cpal=ok
IMPORT GRAPHICS

SUB Main()
  CPALETTE 1, 255, 0, 0
  CPALETTE 2, 0, 255, 0
  PRINT "cpal=ok"
END SUB
