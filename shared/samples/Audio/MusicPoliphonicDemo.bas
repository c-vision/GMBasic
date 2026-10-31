' ============================================================
' FILE: Audio/MusicPoliphonicDemo.bas
' COMPOSIZIONE POLIFONICA — 3 voci simultanee (PLAY LOOP su canali)
' ============================================================
' PLAY LOOP "musica" ON "canale" avvia una voce in un task separato;
' ogni voce usa un voice SDL dedicato → vera polifonia (niente note
' che si interrompono a vicenda).
'
' Fix applicati al motore audio per questo sample:
' - LoadAudioFile/PlayAudioFile ora inizializzano il mixer (prima il
'   suono era silenzioso: Mix_OpenAudio mai chiamato)
' - LoadAudioFile parsa i chunk RIFF (dataStart non è sempre 44:
'   i WAV con chunk extra leggevano PCM spostato → audio distorto)
' - Il VM SOUND ora passa voice+waveform+ADSR+pw a SoundOnVoiceAsync
'   (prima ignorava il voice → tutto su voce 0 → niente polifonia)
' ============================================================

PRINT "Composizione polifonica a 3 voci (12 secondi)..."
PRINT "Basso: arpeggio Do-Sol (ottava bassa)"
PRINT "Armonia: accordi tenuti"
PRINT "Melodia: tema"

' --- Basso: arpeggio continuo (ottava bassa) ---
PLAY LOOP "T100 O2 L8 C G C G E G E G" ON "bass"

' --- Armonia: accordi tenuti a semiminime ---
PLAY LOOP "T100 O3 L4 C E G P4 A F A P4" ON "chord"

' --- Melodia: tema ---
PLAY LOOP "T100 O4 L8 C D E C | E F G2 | A A G F E C | G F E2" ON "lead"

' Lascia suonare 12 secondi
SLEEP 12000

' Ferma tutti i canali
SOUND STOP ALL
PRINT "Fine."
SLEEP 500
