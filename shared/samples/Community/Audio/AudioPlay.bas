' ==============================================================================
' Sample: PLAY
' ==============================================================================

PRINT "1. PLAY standard"
PLAY "T120 O3 L8 CDEFGAB>C"

PRINT "2. PLAY CONCURRENT (accordo)"
PLAY CONCURRENT "O4 C", "O4 E", "O4 G"
SLEEP 2000

PRINT "3. PLAY QUEUE (accoda tre jingle)"
PLAY QUEUE "T200 L8 C E G"
PLAY QUEUE "T200 L8 G F E"
PLAY QUEUE "T200 L4 >C"
SLEEP 4000

PRINT "4. PLAY standard con pause/resume"
PLAY "T160 O3 L8 CDEFGAB>C"
SLEEP 500
SOUND PAUSE
SLEEP 1000
SOUND RESUME
SLEEP 2000
SOUND STOP ALL

PRINT "Completato."
