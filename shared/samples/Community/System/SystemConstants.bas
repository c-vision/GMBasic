' ==============================================================================
' FILE: System/SystemConstants.bas
' Costanti stile PowerBASIC (%NOME): sempre disponibili senza IMPORT.
' ==============================================================================
' Le costanti sono valori interi usati per configurare i comandi:
' - %FILE_CREATED/DELETED/CHANGED/RENAMED → maschere eventi del WATCH (IO)
' - %JOYUP/DOWN/LEFT/RIGHT/FIRE/BUTTON2 → bit del joystick (JOY)
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
' ==============================================================================
IMPORT IO
IMPORT JOY
IMPORT ONKEY

PRINT "=== Costanti IO (file watcher) ==="
PRINT "%FILE_CREATED  = "; %FILE_CREATED
PRINT "%FILE_DELETED  = "; %FILE_DELETED
PRINT "%FILE_CHANGED  = "; %FILE_CHANGED
PRINT "%FILE_RENAMED  = "; %FILE_RENAMED
PRINT "Maschera crea+rinomina = "; %FILE_CREATED + %FILE_RENAMED
PRINT

PRINT "=== Costanti JOY (joystick) ==="
PRINT "%JOYUP      = "; %JOYUP
PRINT "%JOYDOWN    = "; %JOYDOWN
PRINT "%JOYLEFT    = "; %JOYLEFT
PRINT "%JOYRIGHT   = "; %JOYRIGHT
PRINT "%JOYFIRE    = "; %JOYFIRE
PRINT "%JOYBUTTON2 = "; %JOYBUTTON2
PRINT

PRINT "=== Costanti KEY (ON KEY) ==="
PRINT "%K_ESC      = "; %K_ESC
PRINT "%K_ENT    = "; %K_ENT
PRINT "%K_F1       = "; %K_F1
PRINT "%K_F12      = "; %K_F12
PRINT "%K_0       = "; %K_0
PRINT "%K_9       = "; %K_9
PRINT "%K_A        = "; %K_A
PRINT "%K_Z        = "; %K_Z
PRINT "%K_ALT  = "; %K_ALT
PRINT "%K_SHIFT= "; %K_SHIFT
PRINT "%K_CTRL = "; %K_CTRL
PRINT "%K_CTRL_C   = "; %K_CTRL_C
PRINT

PRINT "Completato."
