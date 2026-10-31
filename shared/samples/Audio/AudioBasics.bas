' ==============================================================================
' Sample: Audio Basics — SOUND con waveform e ADSR
' ==============================================================================

PRINT "1. Suono base 440Hz"
SOUND 440, 0.5

PRINT "2. Volume 30%"
SOUND 660, 0.5, 30

PRINT "3. Onda quadra"
SOUND 440, 0.5, 80, 1

PRINT "4. Onda sawtooth"
SOUND 440, 0.5, 80, 2

PRINT "5. Scala DO"
SOUND 262, 0.3
SOUND 294, 0.3
SOUND 330, 0.3
SOUND 349, 0.3
SOUND 392, 0.3
SOUND 440, 0.3
SOUND 494, 0.3
SOUND 523, 0.3

PRINT "Completato."
