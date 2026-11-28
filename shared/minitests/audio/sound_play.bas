' EXPECT: audio ok
' sound_play — SOUND/PLAY/FILTER non sono più un no-op silenzioso: C99Native li emette.
'
' Il test è MUTO di proposito: freq 0 e stringa MML vuota non aprono il device (il runtime
' esce prima di caricare il mixer), quindi non fa rumore e non costa la durata del tono su
' nessuna gamba. Quello che si verifica è che la sintassi completa — voice, timbro, ADSR,
' pulse width, sequenza MML e filtro — ARRIVI al compilatore e che il programma giri.
'
' Prima del 2026-11-28 il parser nativo riduceva queste righe a REM: compilavano senza
' errore e senza suono, e nessuna gamba C vedeva i parametri.

SOUND 0, 0.01
SOUND ON 1, 0, 0.01, 60, 1, 0.01, 0, 1, 0.02, 50
PLAY ""
PLAY CONCURRENT "", ""
FILTER 0, 1, 1000, 50
FILTER 0, 0, 1000, 0

PRINT "audio ok"
