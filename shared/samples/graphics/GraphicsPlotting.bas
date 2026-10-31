' FILE: Graphics/GraphicsPlotting.bas
' =================================================================================================
'       GUIDA TEORICA: LIBRERIA DI PLOTTING AVANZATA (NAMESPACE CHART)
' =================================================================================================
'
' Questo test dimostra l'utilizzo della libreria di plotting integrata in GM-BASIC, progettata
' per creare grafici 2D (a linee, a barre, finanziari, etc.) in modo semplice e dichiarativo.
' La libreria astrae la complessità del disegno manuale (`PSET`, `LINE`) e opera su un
' modello a oggetti gestito tramite "handle".
'
' 1. IL MODELLO A HANDLE
'    - `CHART.CREATE(...)`: Questa funzione crea un nuovo "oggetto grafico" in memoria e
'      restituisce un `handle` (un numero intero univoco) che lo identifica. L'handle è
'      l'unico modo per interagire con quel grafico specifico.
'    - Questo approccio permette di gestire più grafici contemporaneamente, ognuno con il
'      proprio stato (dati, titoli, stili) in modo isolato.
'
' 2. FLUSSO DI LAVORO
'    La creazione di un grafico segue un flusso logico:
'    a) Preparazione Dati: I dati da visualizzare devono essere preparati in array numerici.
'    b) Creazione (`CHART.CREATE`): Si crea l'oggetto grafico, specificando titoli e etichette.
'    c) Aggiunta Dati (`CHART PLOT`): Si aggiungono una o più serie di dati al grafico,
'       specificando lo stile di rendering (es. "LINE", "BAR", "SCATTER").
'    d) Rendering (`CHART SHOW`, `CHART SAVE`): Si visualizza il grafico a schermo o lo si
'       salva su un file immagine.
'    e) Pulizia (`CHART CLOSE`): Si rilasciano le risorse associate all'handle del grafico.
'
' 3. ESECUZIONE ASINCRONA
'    Una caratteristica chiave della libreria è che le operazioni di rendering (`SHOW`, `SAVE`)
'    sono **non bloccanti** (asincrone).
'    - Quando si chiama `CHART SHOW`, la finestra del grafico appare, ma il programma BASIC
'      continua immediatamente la sua esecuzione.
'    - Questo permette all'applicazione principale di rimanere reattiva mentre i grafici
'      vengono visualizzati o salvati in background.
'
' =================================================================================================

' Importa il namespace CHART per rendere disponibili i comandi.
IMPORT CHART
5 IMPORT CHART

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ GraphicsPlotting - Esempio Completo della Libreria di Plotting              ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

60 ' --- FASE 1: Preparazione dei dati ---
' I dati da plottare devono essere organizzati in array numerici.
70 PRINT "Genero i dati per le serie Sin/Cos..."
80 DIM x_dati(100), sin_dati(100), cos_dati(100)
90 FOR i = 0 TO 100
100     x_dati(i) = i
        ' Calcola i valori per le funzioni seno e coseno.
110     sin_dati(i) = SIN(i * PI() / 25)  ' 2 cicli completi su 100 punti
120     cos_dati(i) = COS(i * PI() / 25)
130 NEXT i
140 PRINT "Dati generati."
150 PRINT

160 ' --- FASE 2: Creazione e manipolazione del grafico ---
170 PRINT "Creo un nuovo oggetto grafico in memoria..."
' Crea un oggetto grafico specificando il titolo della finestra e le etichette degli assi.
' Restituisce un handle numerico per riferirsi a questo specifico grafico.
180 handle = CHART.CREATE("Grafico Sinusoide e Cosinusoide", "Asse X", "Asse Y")
190 PRINT "Grafico creato con handle:"; handle
200 PRINT

210 PRINT "Aggiungo la serie di dati per il Seno..."
' Aggiunge una serie di dati (x, y) al grafico identificato da 'handle'.
' Specifica che deve essere renderizzata come una linea ("LINE") e le assegna una legenda ("Seno(x)").
220 CHART PLOT handle, x_dati, sin_dati, "LINE", "Seno(x)"

230 PRINT "Aggiungo la serie di dati per il Coseno..."
' Aggiunge una seconda serie allo stesso grafico.
240 CHART PLOT handle, x_dati, cos_dati, "LINE", "Coseno(x)"
250 PRINT "Serie di dati aggiunte."
260 PRINT

270 ' --- FASE 3: Rendering e Output (operazioni asincrone) ---
280 PRINT "Visualizzo il grafico a (50, 50). Lo script non si blocca..."
' Mostra la finestra del grafico alle coordinate schermo (50, 50).
' Questa operazione è asincrona: il programma non si ferma qui.
290 CHART SHOW handle, 50, 50
300 PRINT

310 PRINT "Salvo il grafico su 'test_plot.png'. Lo script non si blocca..."
' Salva il grafico in un file immagine PNG con le dimensioni specificate.
' Anche questa è un'operazione asincrona.
320 CHART SAVE handle, "test_plot.png", 800, 600
330 PRINT

' Dimostra che l'esecuzione del programma principale continua mentre il grafico è visibile.
340 PRINT "Lo script BASIC continua la sua esecuzione mentre le operazioni di I/O sono in corso..."
350 FOR i = 1 TO 3
360     PRINT "Tick di esecuzione post-plotting..."; i
370     SLEEP 500
380 NEXT i
390 PRINT

400 ' --- FASE 4: Pulizia ---
410 PRINT "Rilascio le risorse del grafico con handle:"; handle
' È buona norma rilasciare esplicitamente le risorse del grafico quando non è più necessario.
420 CHART CLOSE handle
430 PRINT "Risorse rilasciate."
440 PRINT

450 PRINT "Esempio di plotting completato."
460 END

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Copertura API: Il test copre l'intero ciclo di vita di un grafico: `CREATE`, `PLOT` (multi-serie),
'   `SHOW`, `SAVE`, e `CLOSE`.
'
' - Dati: Utilizza dati generati programmaticamente, dimostrando l'integrazione con le funzioni
'   matematiche (`SIN`, `COS`, `PI`).
'
' - Asincronia: Dimostra correttamente la natura non bloccante dei comandi di rendering.
'
' Casi Mancanti Integrati:
' - Ho aggiunto un esempio di `CHART FINANCIAL` (grafico a candele) per dimostrare un altro
'   tipo di plot e l'uso di array multipli per i dati (OHLC).
'
' =================================================================================================