' FILE: Audio/AudioAdvanced.bas
' =================================================================================================
'       GUIDA TEORICA: SISTEMA AUDIO AVANZATO (SOUND & PLAY)
' =================================================================================================
'
' Questo test esplora le funzionalità del sistema audio di GM-BASIC, che fornisce un controllo
' a più livelli sulla riproduzione di suoni e musica.
'
' 1. SINTETIZZATORE DI TONI (`SOUND`)
'    - Sintassi: `SOUND frequenza, durata_secondi`
'    - `SOUND` genera toni semplici specificando la frequenza (in Hz) e la durata.
'    - È un comando **bloccante**: l'esecuzione del programma si ferma fino al termine del suono.
'    - Ideale per effetti sonori immediati o beep.
'
' 2. LINGUAGGIO MUSICALE (`PLAY`)
'    - `PLAY` interpreta una stringa di comandi per eseguire melodie complesse.
'      - `T<bpm>`: Imposta il tempo (battiti al minuto).
'      - `O<n>`: Imposta l'ottava.
'      - `L<n>`: Imposta la durata della nota (1=intera, 4=semiminima, 8=croma, etc.).
'      - `C,D,E,F,G,A,B`: Note musicali. `+` o `#` per diesis, `-` per bemolle.
'      - `>` e `<`: Aumenta/diminuisce l'ottava.
'      - `P<n>`: Pausa (della stessa durata di una nota).
'
' 3. MODALITÀ DI ESECUZIONE DI `PLAY`
'    - `PLAY "..."` (Default): Esecuzione **sincrona/bloccante**. Il programma attende la fine.
'    - `PLAY ... ON "canale"`: Esecuzione **asincrona** su un canale nominato. L'esecuzione
'      del programma continua immediatamente. Permette di avere più suoni/musiche in parallelo.
'    - `PLAY LOOP ... ON "canale"`: Esecuzione asincrona e in loop continuo.
'    - `PLAY CONCURRENT "nota1", "nota2", ...`: Esegue più stringhe `PLAY` contemporaneamente
'      per creare accordi.
'    - `PLAY QUEUE "..."`: Aggiunge una melodia a una coda globale. Le melodie vengono
'      eseguite in sequenza, una dopo l'altra.
'
' 4. COMANDI DI CONTROLLO GLOBALI
'    Questi comandi, prefissati da `SOUND` o `PLAY`, agiscono su tutti i canali o su canali specifici.
'    - `STOP ["canale" | ALL]`: Ferma la riproduzione.
'    - `PAUSE`: Mette in pausa tutti i canali.
'    - `RESUME`: Riprende la riproduzione da dove era stata messa in pausa.
'
' =================================================================================================

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsAudio - Audio Avanzato (SOUND & PLAY)                               ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
5 PRINT

' --- Test 1: SOUND (Sincrono/Bloccante) ---
' Genera una sequenza di toni semplici. L'esecuzione attende la fine di ogni SOUND.
20 PRINT "--- 1. Test SOUND Base ---"
30 PRINT "Eseguo una semplice scala ascendente (bloccante)..."
40 SOUND 261.63, 0.3 ' Do
50 SOUND 293.66, 0.3 ' Re
60 SOUND 329.63, 0.3 ' Mi
70 SOUND 349.23, 0.3 ' Fa
80 PRINT "Scala completata."
90 PRINT "(Premi un tasto per continuare)" : WAITKEY : PRINT

' --- Test 2: PLAY (Sincrono/Bloccante) ---
' Esegue una melodia. Il programma si ferma e attende la fine della riproduzione.
100 PRINT "--- 2. Test PLAY Base (bloccante) ---"
110 PRINT "Eseguo una melodia. Il programma attenderà la fine."
120 PLAY "T180 L8 O4 C D E F G A B > C"
130 PRINT "Melodia terminata."
140 PRINT "(Premi un tasto per continuare)" : WAITKEY : PRINT

' --- Test 3: PLAY Asincrono (ON "canale") e in Loop ---
150 PRINT "--- 3. Test PLAY Asincrono e Loop ---"
160 PRINT "Avvio una musica di sottofondo in loop sul canale 'bgm'."
' PLAY LOOP ... ON "canale" avvia la musica in un thread separato e l'esecuzione prosegue.
170 PLAY LOOP "T120 O3 L4 G E >C P C< G E >C P C<" ON "bgm"
180 PRINT "La musica è partita in background."
190 PRINT "Ora suono un effetto sonoro sul canale 'sfx'..."
200 SLEEP 1000
' Viene eseguito in parallelo alla musica di sottofondo, senza interromperla.
210 PLAY "O6 L16 C E G" ON "sfx"
220 SLEEP 1000
230 PRINT "Ora fermo solo la musica di sottofondo ('bgm')."
' Il comando STOP può specificare un canale target.
240 SOUND STOP "bgm"
250 PRINT "Musica 'bgm' fermata."
260 PRINT "(Premi un tasto per continuare)" : WAITKEY : PRINT

' --- Test 4: PLAY CONCURRENT ---
' Esegue più stringhe PLAY contemporaneamente per creare accordi.
270 PRINT "--- 4. Test PLAY CONCURRENT ---"
280 PRINT "Suono un accordo di Do Maggiore (Do-Mi-Sol) contemporaneamente."
290 PLAY CONCURRENT "O4 C", "O4 E", "O4 G"
300 PRINT "(Premi un tasto per continuare)" : WAITKEY : PRINT

' --- Test 5: PLAY QUEUE ---
' Aggiunge melodie a una coda di riproduzione globale. Vengono eseguite una dopo l'altra.
310 PRINT "--- 5. Test PLAY QUEUE ---"
320 PRINT "Accodo tre brevi jingle. Verranno eseguiti in sequenza."
330 PLAY QUEUE "T200 L8 C E G"
340 PLAY QUEUE "T200 L8 G F E"
350 PLAY QUEUE "T200 L4 >C"
360 PRINT "Jingles accodati. L'esecuzione inizierà in background."
370 PRINT "(Attendi la fine dei jingle...)"
380 SLEEP 4000
390 PRINT "(Premi un tasto per continuare)" : WAITKEY : PRINT

' --- Test 6: Controlli Globali ---
' Verifica il funzionamento di PAUSE, RESUME e STOP ALL.
400 PRINT "--- 6. Test Controlli Globali (PAUSE/RESUME/STOP ALL) ---"
410 PRINT "Avvio una nuova musica di sottofondo in loop sul canale 'music'."
420 PLAY LOOP "T150 O4 L8 C G A G F E D E" ON "music"
430 SLEEP 2000
440 PRINT "Musica in esecuzione. Premi un tasto per metterla in PAUSA..."
450 WAITKEY
' PAUSE mette in pausa TUTTI i suoni attivi.
460 SOUND PAUSE
470 PRINT "AUDIO IN PAUSA. Premi un tasto per riprendere (RESUME)..."
480 WAITKEY
' RESUME riprende tutti i suoni precedentemente in pausa.
490 SOUND RESUME
500 PRINT "AUDIO RIPRESO. Premi un tasto per fermare tutto (STOP ALL)..."
510 WAITKEY
' STOP ALL termina la riproduzione su tutti i canali.
520 SOUND STOP ALL
530 PRINT "Tutto l'audio è stato fermato."
540 PRINT "(Premi un tasto per continuare)" : WAITKEY: PRINT

550 PRINT "--- Test Audio Completato ---"
560 END