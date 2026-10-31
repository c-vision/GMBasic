' FILE: Math/MathFinancial.bas
' ======================================================================================================================
' GUIDA TEORICA: FUNZIONI BUILT-IN COMUNI
' ======================================================================================================================
' Questo programma testa un'ampia gamma di funzioni built-in essenziali per la manipolazione di dati,
' in particolare date, numeri e calcoli finanziari di base.
'
' 1. FUNZIONI DI VALIDAZIONE E CONVERSIONE
'    - ISNUMERIC(expr): Restituisce TRUE se l'espressione può essere interpretata come un numero.
'    - ISDATE(expr): Restituisce TRUE se l'espressione è un tipo o valore di data valido.
'    - DATEVALUE(stringa_data$): Converte una stringa in un valore di tipo DATE.
'
' 2. FUNZIONI DI DATA E ORA
'    - TODAY(): Restituisce la data corrente.
'    - NOW(): Restituisce la data e l'ora correnti.
'    - TIME(): Restituisce una stringa con l'ora corrente ("HH:mm:ss").
'    - WEEKDAY(data): Restituisce il giorno della settimana come numero (1=Dom, ..., 7=Sab).
'    - DATEADD(intervallo$, numero, data): Aggiunge un intervallo a una data (es. "m" per mesi).
'    - DATEDIFF(intervallo$, data1, data2): Calcola la differenza tra due date.
'    - EOMONTH(data): Restituisce l'ultimo giorno del mese per la data specificata.
'
' 3. FUNZIONI MATEMATICHE E DI ARROTONDAMENTO
'    - ROUND(numero, decimali): Arrotonda un numero a un numero specificato di cifre decimali.
'    - TRUNC(numero, decimali): Tronca un numero a un numero specificato di cifre decimali.
'    - CEILING(numero): Arrotonda un numero all'intero superiore più vicino.
'    - FLOOR(numero): Arrotonda un numero all'intero inferiore più vicino.
'
' 4. FUNZIONI DI FORMATTAZIONE
'    - FORMAT(valore, formato$): Formatta un numero o una data in una stringa secondo un pattern specificato.
'      Utilizza le stringhe di formato standard di .NET per massima flessibilità.
'
' 5. FUNZIONI FINANZIARIE DI BASE (Namespace FINANCE)
'    - PMT(tasso, periodi, importo): Calcola la rata di un prestito.
'    - FV(tasso, periodi, rata, importo_iniziale): Calcola il valore futuro di un investimento.
' ======================================================================================================================

' Importa le funzioni finanziarie come PMT, FV, etc.
IMPORT FINANCE
1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ MathFinancial - Funzioni di Utilità e Finanziarie                     ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 ' === TEST FUNZIONI BUILT-IN ===
30 PRINT "--- 1. CONVERSIONI, VALIDAZIONE E ORARIO ---"
40 DIM DATA_TEST AS DATE
' Converte una stringa in un tipo DATE
50 DATA_TEST = DATEVALUE("2025-07-31")
60 PRINT "DATEVALUE('2025-07-31') ="; DATA_TEST
' Controlla se un'espressione è numerica
70 PRINT "ISNUMERIC('123.45') ="; ISNUMERIC("123.45")
' Controlla se un'espressione è una data valida
80 PRINT "ISDATE(DATA_TEST) ="; ISDATE(DATA_TEST)
' Ottiene le informazioni di data/ora correnti
90 PRINT "OGGI ="; TODAY()
100 PRINT "ADESSO ="; NOW()
110 PRINT "ORA$ = "; TIME()
120 PRINT
130
140 PRINT "--- 2. FORMATTAZIONE ---"
' Formatta un numero con separatore delle migliaia e due decimali
150 PRINT "FORMAT(12345.67, '#,##0.00') ="; FORMAT(12345.67, "#,##0.00")
' Formatta una data in un formato esteso
160 PRINT "FORMAT(TODAY(), 'dddd, dd MMMM') ="; FORMAT(TODAY(), "dddd, dd MMMM")
170 PRINT
180
190 PRINT "--- 3. ARITMETICA ---"
' Arrotonda un valore
200 PRINT "ROUND(123.456, 2) ="; ROUND(123.456, 2)
' Tronca un valore
210 PRINT "TRUNC(98.765, 1) ="; TRUNC(98.765, 1)
' Arrotonda per eccesso
220 PRINT "CEILING(45.1) ="; CEILING(45.1)
' Arrotonda per difetto
230 PRINT "FLOOR(45.9) ="; FLOOR(45.9)
240 PRINT
250
260 PRINT "--- 4. CALCOLI SU DATE ---"
' Ottiene il giorno della settimana
270 PRINT "GIORNO SETT. (31/07/25 è Gio=5):"; WEEKDAY(DATA_TEST)
280 DIM SCADENZA AS DATETIME
' Aggiunge 3 mesi a una data
290 SCADENZA = DATEADD("m", 3, DATA_TEST)
300 PRINT "3 mesi dopo il 31/07/25 ="; FORMAT(SCADENZA, "dd/MM/yyyy")
' Calcola la differenza in giorni
310 GIORNI_DIFF = DATEDIFF("d", DATA_TEST, SCADENZA)
320 PRINT "GIORNI DI DIFFERENZA ="; GIORNI_DIFF
330 DIM FINE_MESE AS DATE
' Calcola l'ultimo giorno del mese
340 FINE_MESE = EOMONTH(DATA_TEST)
350 PRINT "FINE MESE LUGLIO 2025 ="; FORMAT(FINE_MESE, "dd/MM/yyyy")
360 PRINT
370
380 PRINT "--- 5. FINANZIARIE (Mutuo 10000, 5%, 3 anni) ---"
390 TASSO_ANNUO = 0.05
400 ANNI = 3
410 IMPORTO = 10000
' Calcola la rata del mutuo (PMT)
420 RATA = PMT(TASSO_ANNUO / 12, ANNI * 12, IMPORTO)
430 PRINT "RATA MENSILE ="; FORMAT(RATA, "#,##0.00")
' Calcola il valore futuro (dovrebbe essere zero se la rata è corretta)
440 VAL_FUTURO = FV(TASSO_ANNUO / 12, ANNI * 12, RATA, IMPORTO)
450 PRINT "VALORE FUTURO (dovrebbe essere ~0) ="; VAL_FUTURO
460 PRINT
470 PRINT "--- TEST COMPLETATO ---"
480 END