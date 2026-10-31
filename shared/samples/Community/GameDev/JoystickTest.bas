IMPORT JOY
' JoystickTest.bas — JOY() Gamepad Test
' JOY(port%) returns bitmask: UP=1, DOWN=2, LEFT=4, RIGHT=8, FIRE=16, BUTTON2=32
' Constants: %JOYUP, %JOYDOWN, %JOYLEFT, %JOYRIGHT, %JOYFIRE (sempre disponibili senza IMPORT)

PRINT "=== JOYSTICK/GAMEPAD Test ==="
PRINT "Connect a gamepad and press buttons"
PRINT "JOYCOUNT ="; PORTS()
PRINT ""
PRINT "Joysticks found:"
DIM i% = 0
WHILE i% < PORTS()
    PRINT "  Port" + STR$(i%) + ": '" + NAME$(i%) + "'"
    i% = i% + 1
WEND

IF PORTS() = 0 THEN
    PRINT "No joysticks connected."
    END
END IF

PRINT ""
PRINT "Polling joystick 0 for 100 iterations..."
PRINT "Direction bits: UP=1 DOWN=2 LEFT=4 RIGHT=8 FIRE=16"
PRINT ""

DIM iter% = 0
DIM state% = 0
DIM lastState% = -1

WHILE iter% < 100
    state% = MASK(0)
    IF state% <> lastState% THEN
        PRINT "MASK(0) ="; state%;
        IF state% AND %JOYUP    THEN PRINT " UP";
        IF state% AND %JOYDOWN  THEN PRINT " DOWN";
        IF state% AND %JOYLEFT  THEN PRINT " LEFT";
        IF state% AND %JOYRIGHT THEN PRINT " RIGHT";
        IF state% AND %JOYFIRE  THEN PRINT " FIRE";
        PRINT ""
        lastState% = state%
    END IF
    iter% = iter% + 1
WEND

PRINT ""
PRINT "Test complete."
