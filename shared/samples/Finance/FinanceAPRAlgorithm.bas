' FILE: Finance/FinanceMathAPRAlgorithm.bas
' ======================================================================================================================
' GUIDA TEORICA: FUNZIONI FINANZIARIE E ALGORITMI NUMERICI
' ======================================================================================================================
' Questo programma dimostra due aspetti importanti di GM-BASIC: l'implementazione di algoritmi numerici complessi
' e l'utilizzo di funzioni finanziarie built-in per calcoli standard.
'
' 1. ALGORITMI NUMERICI: IL CALCOLO DELL'APR
'    - La funzione `FNAPR` implementa l'algoritmo di Newton-Raphson, un metodo iterativo per trovare le radici di
'      una funzione (in questo caso, per trovare il tasso di interesse che azzera la funzione del valore attuale).
'    - Questo dimostra la capacità del linguaggio di gestire calcoli in virgola mobile ad alta precisione,
'      operatori matematici (potenza `^`, valore assoluto `ABS`) e cicli `WHILE` per la convergenza.
'
' 2. FUNZIONI FINANZIARIE BUILT-IN (Namespace FINANCE)
'    - GM-BASIC fornisce un set di funzioni finanziarie standard per semplificare calcoli comuni.
'    - `SLN(cost, salvage, life)`: Calcola l'ammortamento a quote costanti (Straight-Line).
'    - `SYD(cost, salvage, life, per)`: Calcola l'ammortamento a somma degli anni (Sum-of-Years' Digits) per un
'      periodo specifico.
'    - `NPV(rate, values...)`: Calcola il Valore Attuale Netto (Net Present Value) di una serie di flussi di cassa
'      futuri, scontati a un tasso fisso. NOTA: Non include il flusso al tempo 0 (investimento iniziale).
'    - `IRR(values[], guess)`: Calcola il Tasso Interno di Rendimento (Internal Rate of Return) per una serie di
'      flussi di cassa (incluso l'investimento iniziale negativo). Richiede una stima iniziale.
' ======================================================================================================================

' Importa il namespace che contiene le funzioni finanziarie (SLN, SYD, NPV, IRR)
IMPORT FINANCE

REM Math.APR Algorithm.bas
REM --- ESEMPIO DI ALGORITMO FINANZIARIO (APR) ---
REM Adattato da un dialetto Business BASIC, questo esempio
REM calcola il Tasso Annuo Effettivo Globale (TAEG/APR).

FUNCTION FNAPR(NUM_PMTS AS DOUBLE, POINTS AS DOUBLE, RATE AS DOUBLE) AS DOUBLE
    REM --- PARAMETRI ---
    REM NUM_PMTS: Numero di pagamenti mensili (es. 360 per 30 anni)
    REM POINTS:   Punti percentuali pagati in anticipo (es. 1 per 1%)
    REM RATE:     Tasso di interesse annuale nominale (es. 10 per 10%)
    
    ' Importo nominale del prestito (costante per il calcolo)
    FACE_AMT = 100 
    ' Frequenza dei pagamenti (mensile)
    PMT_FREQ = 12
    ' Ammontare dei punti pagati
    POINT_AMT = FACE_AMT * (POINTS / 100)
    ' Capitale netto ricevuto
    NET_PRIN = FACE_AMT - POINT_AMT
    ' Tasso di interesse periodico (mensile)
    RATE_FACTOR = (RATE / 100) / PMT_FREQ
    
    ' Calcolo della rata mensile (PMT) usando la formula standard
    PMT_AMT = FACE_AMT / (1 / RATE_FACTOR - (1 / (RATE_FACTOR * (1 + RATE_FACTOR) ^ NUM_PMTS)))
    
    REM --- CALCOLO ITERATIVO APR (METODO NEWTON-RAPHSON CORRETTO) ---
    I = RATE_FACTOR ' Stima iniziale migliore per il tasso effettivo
    I_OLD = 0
    
    ' Itera finché la differenza tra due stime successive non è trascurabile
    WHILE ABS(I - I_OLD) > 0.0000001
      I_OLD = I
      ' Calcola il Valore Attuale (PV) usando la stima corrente del tasso (I)
      PV_CALC = PMT_AMT * (1 - (1 + I) ^ -NUM_PMTS) / I
      ' La funzione f(i) che vogliamo azzerare: la differenza tra PV calcolato e il capitale netto
      F_I = PV_CALC - NET_PRIN
      
      ' Calcola la derivata prima della funzione del Valore Attuale (f'(i))
      ' Questo termine rappresenta la sensibilità del PV a una variazione del tasso
      F_PRIME_I = ( ( NUM_PMTS * PMT_AMT * (1+I)^(-NUM_PMTS-1) * I ) - ( PMT_AMT * (1-(1+I)^-NUM_PMTS) ) ) / (I^2)
      
      ' Applica la formula di Newton-Raphson per ottenere la stima successiva del tasso
      I = I - F_I / F_PRIME_I
    WEND
    
    ' L'APR è il tasso periodico effettivo (I) annualizzato
    FNAPR = I * PMT_FREQ * 100
END FUNCTION

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ MathAPRAlgorithm - Calcolo APR e Funzioni Finanziarie                   ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 REM --- PROGRAMMA PRINCIPALE ---
30 PRINT "Calcolo APR per un mutuo a 30 anni (360 rate) al 10% con 1 punto..."
40 PRINT
50 APR = FNAPR(360, 1, 10)
60 PRINT "Tasso Annuo Effettivo Globale (APR) calcolato: ";
: PRINT APR; "%"
70 PRINT 

' === TEST NUOVE FUNZIONI FINANZIARIE (SLN, SYD, NPV, IRR) ===

110 PRINT "--- 1. Ammortamento a Quote Costanti (SLN) ---"
120 COSTO = 10000 : RESIDUO = 1000 : VITA = 10
130 AMMORTAMENTO_ANNUO = SLN(COSTO, RESIDUO, VITA)
140 PRINT "Costo: "; COSTO; " | Residuo: "; RESIDUO; " | Vita: "; VITA; " anni"
150 PRINT "Ammortamento annuale (SLN): "; AMMORTAMENTO_ANNUO; " (Atteso: 900)"
160 PRINT

170 PRINT "--- 2. Ammortamento a Somma degli Anni (SYD) ---"
180 PRINT "Uso gli stessi dati. Calcolo per il primo e l'ultimo anno."
190 AMMORT_ANNO_1 = SYD(COSTO, RESIDUO, VITA, 1)
200 AMMORT_ANNO_10 = SYD(COSTO, RESIDUO, VITA, 10)
' Formatta l'output a due cifre decimali per leggibilità
210 PRINT "Ammortamento Anno 1 (SYD): "; FORMAT(AMMORT_ANNO_1, "#.00"); " (Atteso: 1636.36)"
220 PRINT "Ammortamento Anno 10 (SYD): "; FORMAT(AMMORT_ANNO_10, "#.00"); " (Atteso: 163.64)"
230 PRINT

240 PRINT "--- 3. Valore Attuale Netto (NPV) ---"
250 TASSO_SCONTO = 0.08
260 PRINT "Investimento iniziale (non in NPV): -10000"
270 PRINT "Flussi di cassa futuri: 3000, 4200, 6800"
280 PRINT "Tasso di sconto: "; TASSO_SCONTO * 100; "%"
' NPV calcola il valore attuale dei flussi futuri (escluso l'investimento iniziale)
290 VAL_ATT_NETTO = NPV(TASSO_SCONTO, 3000, 4200, 6800)
300 PRINT "Valore flussi scontati (NPV): "; FORMAT(VAL_ATT_NETTO, "#.00")
' Il risultato finale del progetto si ottiene sottraendo l'investimento iniziale
310 PRINT "Risultato finale (NPV - Investimento): "; FORMAT(VAL_ATT_NETTO - 10000, "#.00"); " (Atteso: 1776.81)"
320 PRINT

330 PRINT "--- 4. Tasso Interno di Rendimento (IRR) ---"
' L'array per IRR deve includere l'investimento iniziale (negativo)
340 DIM FLUSSI(3) ' Indici da 0 a 3, per 4 periodi
350 FLUSSI(0) = -10000 ' Investimento iniziale
360 FLUSSI(1) = 3000
370 FLUSSI(2) = 4200
380 FLUSSI(3) = 6800
' IRR richiede una stima iniziale del tasso
390 TIR = IRR(FLUSSI, 0.10)
' Il risultato è il tasso che rende l'NPV del progetto uguale a zero
400 PRINT "Tasso Interno di Rendimento (IRR): "; FORMAT(TIR * 100, "#.####"); "%"