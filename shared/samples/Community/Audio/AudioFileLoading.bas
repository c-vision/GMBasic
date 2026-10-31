' ==============================================================================
' Sample: SOUND LOAD / SOUND PLAY — Audio file playback
' ==============================================================================
' Carica e riproduce file audio WAV.
'
' Comandi:
'   SOUND LOAD "file.wav" AS #handle   — carica un file WAV
'   SOUND PLAY #handle                  — riproduce il file caricato
'   SOUND VOLUME 50                     — imposta volume master (0-100)
'   SOUND VOLUME 75, 0                  — imposta volume del canale 0
' ==============================================================================

' Carica un effetto sonoro
SOUND LOAD "effect.wav" AS #sfx
PRINT "File audio caricato!"

' Imposta il volume al 70%
SOUND VOLUME 70

' Riproduce l'effetto sonoro
PRINT "Riproduco effetto sonoro..."
SOUND PLAY #sfx

PRINT "Audio riprodotto con successo!"
