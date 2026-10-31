' BUTTON: click REALE -> callback CLICK, verificato end-to-end su C99Native.
' Il click e' iniettato dal present (GMB_FORM_AUTOCLICK="30,150,70") al frame 30:
'   GMB_FORM_AUTOCLICK="30,150,70" tools/gmb_c99native_run.sh \
'       native/gmb_runtime_c/tests/gfx_button_click.bas
' Atteso: "clicks= 1" (prima del fix della doppia scala era 0: 150,70 -> 75,35).
IMPORT CONTROLS
SCREEN 12, 640, 320, 32
DIM clicks AS INTEGER
SUB OnClick()
    clicks = clicks + 1
END SUB
BUTTON 60, 50, 200, 44, "Cliccami" ON CLICK OnClick
DIM f AS INTEGER
FOR f = 1 TO 120
    WINDOW PUMP 5
NEXT
SCREEN 0
PRINT "clicks="; clicks
