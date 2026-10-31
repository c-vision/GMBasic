' Minitest controlli grafico-only (pixel-control path, no-op su TEXT) — parità 4 backend.
IMPORT CONTROLS
SUB Main()
  CANVAS 0, 0, 40, 20
  IMAGE 0, 24, 32, 24, "logo.png"
  IMAGEBUTTON 40, 24, 48, 24, "Salva"
  QR 0, 52, 24, 24, "https://gmbasic"
  RICHTEXT 0, 80, 60, 24, "Titolo|riga 1|riga 2"
  HALFBLOCK 0, 108, 40, 16
  TILING 0, 128, 40, 16
  PRINT "OK-gfx-controls"
END SUB
