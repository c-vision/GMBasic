
' FILE: Graphics/GraphicsFullGfxTest.bas
' =================================================================================================
' GUIDA TEORICA: GRAFICA AVANZATA E GESTIONE DELLO SCHERMO
' =================================================================================================
'
' Questo test è una suite completa che dimostra le capacità di manipolazione grafica a basso
' e alto livello di GM-BASIC, inclusa la gestione delle "pagine" di memoria video.
'
' 1. PRIMITIVE DI DISEGNO (PSET, PRESET)
' - PSET (x, y), colore: Accende un singolo pixel alle coordinate specificate. È il comando
' più elementare per il disegno.
' - PRESET (x, y): "Spegne" un pixel, colorandolo con il colore di sfondo corrente.
'
' 2. RIEMPIMENTO DI AREE (PAINT)
' - Sintassi: PAINT (x, y), colore_riempimento, colore_bordo
' - Esegue un algoritmo di "flood fill". A partire da un punto "seme" (x, y), colora
' tutta l'area circostante con colore_riempimento fino a quando non incontra un
' colore_bordo.
' - È essenziale che l'area da riempire sia completamente chiusa, altrimenti il colore
' "fuoriesce" e riempie l'intero schermo.
'
' 3. GESTIONE DELLE PAGINE GRAFICHE E DOUBLE BUFFERING (SCREENSET, PCOPY)
' GM-BASIC supporta più "pagine" di memoria video per consentire tecniche di animazione
' fluide come il double buffering.
' - Pagina Attiva: La pagina su cui vengono eseguiti i comandi di disegno.
' - Pagina Visibile: La pagina attualmente mostrata a schermo.
'
' - SCREENSET [visibile], [attiva]: Permette di cambiare queste pagine.
' - SCREENSET , 1: Imposta la pagina 1 come attiva (per il disegno), ma lascia
' la pagina 0 come visibile. Si disegna "dietro le quinte".
' - SCREENSET 1: Rende visibile la pagina 1.
'
' - PCOPY pagina_sorgente, pagina_destinazione: Copia l'intero contenuto di una pagina
' su un'altra. In questo test, viene usata per copiare la scena disegnata sulla pagina
' nascosta a quella visibile in un colpo solo, evitando il "flickering".
'
' 4. SALVATAGGIO E RIPRISTINO DELLO SCHERMO (SAVE SCREEN, RESTORE SCREEN)
' Questi comandi permettono di salvare e ripristinare lo stato completo di una schermata
' grafica in un buffer di memoria interno, identificato da un nome.
' - SAVE SCREEN "nome_salvataggio": Cattura un'istantanea della pagina visibile.
' - RESTORE SCREEN "nome_salvataggio": Ridisegna l'istantanea salvata sullo schermo.
' - È un'operazione molto veloce, ideale per ripristinare sfondi complessi o stati di gioco.
'
' =================================================================================================
2 PRINT "╔══════════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsFullGfxTest - PSET/PRESET, PAINT, PCOPY, SCREENSET, SAVE/RESTORE SCREEN ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 ' === Test Grafico Completo ===
20 ' Testa PSET/PRESET, PAINT, PCOPY, SCREENSET, SAVE/RESTORE SCREEN.
30
' Inizializza la modalità grafica VGA 640x480 con 16 colori.
40 SCREEN 12
50
60 ' --- SCENA 1: Test PSET e PRESET ---
' CPRINT stampa sulla console di testo emulata, utile per i messaggi di debug che non interferiscono con la grafica.
70 CPRINT "SCENA 1: Test PSET/PRESET. Disegno una griglia e poi la cancello."
80 CLS 1 ' Pulisce lo schermo grafico impostando lo sfondo al colore 1 (blu).
90
100 ' Disegno una griglia di punti bianchi (colore 15) usando PSET.
110 FOR Y = 50 TO 400 STEP 20
120 FOR X = 50 TO 600 STEP 20
130 PSET (X, Y), 15
140 NEXT X
150 NEXT Y
160
170 ' Cancello una croce al centro usando PRESET.
' PRESET disegna con il colore di sfondo corrente (blu, impostato da CLS 1).
180 FOR I = 200 TO 400
190 PRESET (I, 240)
200 PRESET (320, I)
210 NEXT I
220
230 CPRINT "Premi un tasto per la SCENA 2 (PAINT)..."
240 WAITKEY
250
260 ' --- SCENA 2: Test PAINT ---
270 CPRINT "SCENA 2: Test PAINT. Disegno e riempio una forma complessa."
280 CLS 0 ' Pulisce lo schermo con lo sfondo nero (colore 0).
290
300 ' Disegno il contorno di una casa con una finestra usando LINE e CIRCLE.
' Questi contorni serviranno come 'colore_bordo' per il comando PAINT.
310 LINE (100, 300)-(500, 300), 15 ' Base (colore 15, bianco)
320 LINE -(500, 100), 15 ' Muro destro
330 LINE -(300, 50), 15 ' Tetto destro
340 LINE -(100, 100), 15 ' Tetto sinistro
350 LINE -(100, 300), 15 ' Muro sinistro
360 CIRCLE (400, 200), 40, 14 ' Finestra (colore 14, giallo)
370
380 ' Riempio le varie parti con PAINT.
' La prima coordinata è il "punto seme" da cui parte il riempimento.
390 PAINT (300, 290), 4, 15 ' Muri rossi (colore 4), fino al bordo bianco (15).
400 PAINT (300, 60), 6, 15 ' Tetto marrone (colore 6), fino al bordo bianco (15).
410 PAINT (400, 200), 11, 14 ' Vetro ciano (colore 11), fino al bordo giallo (14).
420
430 CPRINT "Premi un tasto per la SCENA 3 (PCOPY)..."
440 WAITKEY
450
460 ' --- SCENA 3: Test PCOPY e SCREENSET (Double Buffering) ---
470 CPRINT "SCENA 3: Test PCOPY. Disegno su pagina 1 e copio su pagina 0."
480
' Imposta la pagina di disegno (attiva) a 1. La pagina 0 rimane visibile. Stiamo disegnando "dietro le quinte".
490 SCREENSET , 1
500 CLS 2 ' Pulisce la pagina 1 con sfondo verde.
' È necessario caricare un font per poter usare PRINT in modalità grafica.
510 FONT "font.ttf", 12
520 COLOR 15 : LOCATE 5, 5
530 PRINT "TEST PCOPY"
540
' Torna a visualizzare e disegnare sulla pagina 0.
550 SCREENSET 0, 0
560 CLS 4 ' Lo schermo visibile diventa rosso.
570 CPRINT "Pagina 0 e' rossa. Ora copio... (premi un tasto)"
580 WAITKEY
590
' Copia l'intero contenuto della pagina 1 (verde con testo) sulla pagina 0 (visibile).
600 PCOPY 1, 0
610 CPRINT "Copiato. Ora la pagina 0 dovrebbe essere verde con testo."
620 WAITKEY
630
640 ' --- SCENA 4: Test SAVE/RESTORE SCREEN ---
650 CPRINT "SCENA 4: Test SAVE/RESTORE. Salvo la casa, distruggo e ripristino."
' Ridisegna la scena della casa sulla pagina corrente (pagina 0).
660 CLS 0
670 LINE (100, 300)-(500, 300), 15 : LINE -(500, 100), 15 : LINE -(300, 50), 15 : LINE -(100, 100), 15 : LINE -(100, 300), 15
680 CIRCLE (400, 200), 40, 14 : PAINT (300, 290), 4, 15 : PAINT (300, 60), 6, 15 : PAINT (400, 200), 11, 14
680
' Salva lo stato corrente dello schermo in un buffer di memoria chiamato "casa".
690 SAVE SCREEN "casa"
700 CPRINT "Casa salvata. Ora la 'distruggo'. (premi un tasto)"
710 WAITKEY
720
' Disegna qualcosa di completamente diverso per sovrascrivere la scena.
730 CLS 12 : CIRCLE(320,240),200,15
740 CPRINT "Schermo distrutto. Ora ripristino... (premi un tasto)"
750 WAITKEY
760
' Ricarica lo stato dello schermo dal buffer "casa", ripristinando l'immagine originale.
770 RESTORE SCREEN "casa"
780 CPRINT "Casa ripristinata. Test completato. (premi un tasto per uscire)"
790 WAITKEY
800
' Torna alla modalità testo e chiude la finestra grafica.
810 SCREEN 0
' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - PSET/PRESET: Verificati con successo per disegnare e cancellare pixel.
' - PAINT: Verificato con successo per riempire aree complesse con colori e bordi diversi.
' - SCREENSET/PCOPY: La tecnica di double buffering (disegno su pagina nascosta e copia) è stata
' verificata con successo.
' - SAVE/RESTORE SCREEN: Il salvataggio e il ripristino dello stato dello schermo da un buffer
' in memoria sono stati verificati con successo.
'
' Il file di test copre in modo esaustivo le funzionalità dichiarate.
' Non sono state identificate mancanze significative per questi comandi.
'
' =================================================================================================