' FILE: Graphics/GraphicsMandelbrot.bas
' =================================================================================================
'       GUIDA TEORICA: RENDERING DI FRATTALI - L'INSIEME DI MANDELBROT
' =================================================================================================
'
' Questo programma dimostra la potenza computazionale e grafica di GM-BASIC generando una
' visualizzazione dell'insieme di Mandelbrot, uno dei frattali più famosi.
'
' 1. IL CONCETTO MATEMATICO
'    L'insieme di Mandelbrot è un insieme di numeri complessi 'c' per i quali la successione
'    definita da:
'
'        Z_n+1 = (Z_n)^2 + c
'
'    partendo da Z_0 = 0, non diverge all'infinito. In altre parole, un punto 'c' appartiene
'    all'insieme se la magnitudine di Z_n rimane limitata per tutte le iterazioni.
'
' 2. L'ALGORITMO DI RENDERING
'    - Mappatura Coordinate: Ogni pixel (Px, Py) sullo schermo viene mappato a un punto (Cr, Ci)
'      nel piano complesso. Questo programma mappa la larghezza dello schermo all'intervallo
'      reale [-2.0, 1.0] e l'altezza all'intervallo immaginario [-1.5, 1.5].
'
'    - Test di Fuga (Escape Time Algorithm): Per ogni punto 'c', si itera la formula partendo da Z=0.
'      Si controlla ad ogni passo se la magnitudine di Z (distanza dall'origine) supera una
'      soglia (tipicamente 2).
'      - Se la magnitudine supera 2, il punto "fugge" all'infinito e non appartiene all'insieme.
'        Il numero di iterazioni necessarie per fuggire determina il colore del pixel.
'      - Se dopo un numero massimo di iterazioni (`MaxIter`) la magnitudine è ancora <= 2, si assume
'        che il punto appartenga all'insieme e viene colorato di nero.
'
' 3. OTTIMIZZAZIONE E PERFORMANCE
'    La generazione di frattali è un'operazione computazionalmente intensiva. Questo programma,
'    con i suoi tre cicli `FOR` annidati, è un eccellente stress test per le performance della
'    VM (Virtual Machine) dell'interprete e per l'efficacia del `LoopFlatteningOptimizer`.
'
' =================================================================================================

' Stampa un'intestazione sulla console di avvio
2 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ GraphicsMandelbrot - Frattale di Mandelbrot                                 ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
5 PRINT
' Imposta la modalità grafica VGA 640x480 con 16 colori
10 SCREEN 12

' Definisce le coordinate del piano complesso da visualizzare.
' L'area standard per una buona visualizzazione di Mandelbrot.
30 MinR = -2.0  ' Valore reale minimo
50 MaxR = 1.0   ' Valore reale massimo
60 MinI = -1.5  ' Valore immaginario minimo
70 MaxI = 1.5   ' Valore immaginario massimo

' Numero massimo di iterazioni per punto. Valori più alti aumentano il dettaglio ma rallentano il calcolo.
90 MaxIter = 32

' Cicla su ogni pixel dello schermo (Py = riga, Px = colonna).
110 FOR Py = 0 TO 479
120     FOR Px = 0 TO 639
            ' Converte la coordinata del pixel (Px, Py) in un numero complesso C = Cr + i*Ci.
            ' Questa è la mappatura lineare tra lo spazio dei pixel e il piano complesso.
140         Cr = MinR + Px * (MaxR - MinR) / 639
150         Ci = MinI + Py * (MaxI - MinI) / 479

            ' Inizia l'iterazione Z = Z^2 + C partendo da Z = 0 (Zr=0, Zi=0).
170         Zr = 0
180         Zi = 0
            DIM I AS INTEGER
            
            ' Il ciclo interno esegue l'algoritmo di Mandelbrot.
190         FOR I = 1 TO MaxIter
                ' Calcola Z^2: (Zr + i*Zi)^2 = (Zr^2 - Zi^2) + (2*Zr*Zi)i
210             ZrN = Zr * Zr - Zi * Zi + Cr ' Parte reale del nuovo Z
220             ZiN = 2 * Zr * Zi + Ci       ' Parte immaginaria del nuovo Z
                
                ' Test di fuga: Se la magnitudine al quadrato (Zr^2 + Zi^2) è > 4, il punto diverge.
                ' Si usa il quadrato per evitare il calcolo costoso di una radice quadrata (SQR).
240             IF ZrN * ZrN + ZiN * ZiN > 4 THEN EXIT FOR
                
                ' Aggiorna Z per la prossima iterazione.
250             Zr = ZrN
260             Zi = ZiN
270         NEXT I
            
            ' Sceglie il colore in base a quante iterazioni ci sono volute per "fuggire".
            DIM C AS INTEGER
290         IF I > MaxIter THEN ' Corretto da 'I = MaxIter' a 'I > MaxIter' per coerenza con FOR...NEXT
                ' Se il ciclo è terminato senza EXIT, il punto è nell'insieme.
310             C = 0 ' Colore Nero
320         ELSE
                ' Se il ciclo è stato interrotto, il punto è fuori. Il colore dipende dalla velocità di fuga.
                ' L'operatore MOD garantisce che il colore sia nel range dei 15 colori disponibili (1-15).
340             C = (I MOD 15) + 1
350         ENDIF
            
            ' Disegna il pixel con il colore calcolato.
370         PSET (Px, Py), C
380     NEXT Px
390 NEXT Py

' Attende un tasto prima di terminare e chiudere la finestra grafica.
410 WAITKEY
420 END

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Obiettivo del Test: Il programma dimostra con successo l'uso di cicli `FOR` profondamente
'   annidati e di calcoli matematici a virgola mobile per generare una grafica complessa.
'
' - Funzionalità Verificate: `SCREEN`, `FOR...NEXT` (annidato), `IF...THEN...ELSE`, `EXIT FOR`,
'   `PSET`, operatori matematici, gestione di variabili `DOUBLE` e `INTEGER`.
'
' - Casi Mancanti: Nessuno. Il file serve come un eccellente benchmark algoritmico e non
'   è progettato per testare una singola funzionalità isolata in modo esaustivo.
'
' =================================================================================================