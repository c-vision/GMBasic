' FILE: Samples/Clipper/ClipperStatusBar.bas
' Test STATUSBAR control (comando del namespace TUI)

IMPORT CONTROLS

SUB Main()
    @ 5, 10 SAY "This is a test screen"
    @ 7, 10 SAY "The status bar should appear"
    @ 9, 10 SAY "at the bottom of the screen"
    
    STATUSBAR " Ready | F1=Help | F10=Quit"
    
    PRINT ""
    PRINT "Status bar displayed at bottom."
END SUB
