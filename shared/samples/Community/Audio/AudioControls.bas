' ==============================================================================
' Sample: Audio Controls — Volume, Filter, Stop, Load/Play
' ==============================================================================

PRINT "1. Volume master 50%"
SOUND VOLUME 50
SOUND 440, 0.3

PRINT "2. Volume master 80%"
SOUND VOLUME 80
SOUND 660, 0.3

PRINT "3. Filtro LowPass 1000Hz"
FILTER 0, 1, 1000, 50
SOUND 440, 0.5

PRINT "4. Filtro HighPass 2000Hz"
FILTER 0, 2, 2000, 50
SOUND 880, 0.5

PRINT "5. Filtro OFF"
FILTER 0, 0, 0, 0
SOUND 440, 0.3

PRINT "6. Stop selettivo canale"
PLAY "T140 O3 L4 C E G C" ON "ch1"
PLAY "T140 O3 L4 G B D G" ON "ch2"
SLEEP 500
SOUND STOP "ch1"
SLEEP 1500
SOUND STOP "ch2"

PRINT "7. STOP ALL"
PLAY "T160 O3 L4 C D E F G A B >C" ON "bgm"
PLAY "T160 O4 L8 C C C C" ON "beat"
SLEEP 1000
SOUND STOP ALL

PRINT "8. Caricamento WAV"
SOUND LOAD "effect.wav" AS #sfx
SOUND PLAY #sfx

PRINT "Completato."
