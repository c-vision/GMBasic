' -- GraphicsControlsDemo.bas ---------------------------------------------------
' Dimostra tutti i controlli SOLO-GRAFICA del namespace CONTROLS su SCREEN grafico.
' In TEXT sono no-op; qui li dichiara e li modifica via CONTROL SET/GET per nome.
' Parità 4 backend: IL / PCode / VM-C / C99Native.
' ------------------------------------------------------------------------------
SCREEN 1
IMPORT CONTROLS

SUB Main()
  CANVAS 0, 0, 320, 200 NAME "sfondo"

  IMAGE 8, 8, 64, 64, "logo.png" NAME "img"
  IMAGEBUTTON 80, 8, 96, 32, "Salva" NAME "btnImg"
  QR 184, 8, 48, 48, "https://esempio.test" NAME "qr"

  RICHTEXT 8, 80, 260, 56, "Titolo principale|riga di dettaglio 1|riga di dettaglio 2" NAME "rt"

  HALFBLOCK 8, 144, 120, 40 NAME "hb"
  TILING 140, 144, 120, 40 NAME "tile"

  ' Proprietà comuni via CONTROL SET/GET (stesse su tutti i backend).
  CONTROL SET "qr", "color", 0, "W+/B"
  CONTROL SET "btnImg", "tooltip", 0, "Salva il documento"
  CONTROL SET "rt", "visible", 1
  CONTROL SET "tile", "enabled", 1

  CPRINT "OK-graphics-controls"
END SUB
