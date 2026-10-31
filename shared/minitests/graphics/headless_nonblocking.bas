' MiniTest: comandi senza-namespace che NON bloccano l'harness in-process e non
' richiedono librerie audio/display (verificato 2026-09-22 con bisect: PIXEL BUFFER,
' BLOAD, PALETTE, SAVE SCREEN). Girano headless e completano su IL/PCode/VMC.
' Esclusi da QUESTO file: SOUND/PLAY (richiedono SDL2_mixer, assente nell'harness IL);
' i comandi grafici che APPENDONO sono in `gfx_parked_perleg.bas` (escluso).
' EXPECT: commands-ok
SUB Main()
  DIM buf AS LONG
  PIXEL BUFFER 4, 4, buf
  BLOAD (0, 0), buf
  PALETTE 1, 255, 0, 0
  SAVE SCREEN "work/scr.prg"
  PSET (1, 1), 4
  PRINT "commands-ok"
END SUB