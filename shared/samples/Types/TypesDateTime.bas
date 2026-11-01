' FILE: Types/TypesDateTime.bas
' ======================================================================================================================
' GUIDA TEORICA: TIPI DI DATO PER DATA E ORA
' ======================================================================================================================
' GM-BASIC fornisce un set ricco di tipi di dato per una gestione precisa e robusta delle informazioni temporali.
'
' 1. TIPI DI DATO FONDAMENTALI
'    - `DATE`: Memorizza solo la parte della data (giorno, mese, anno).
'    - `TIME`: Memorizza solo la parte dell'ora (ore, minuti, secondi, millisecondi).
'    - `DATETIME`: Memorizza sia la data che l'ora.
'    - `TIMESPAN`: Rappresenta una durata o un intervallo di tempo (es. "10 giorni e 5 ore").
'
' 2. CREAZIONE DI VALORI
'    - Funzioni di Sistema: `TODAY()` (restituisce `DATE`), `NOW()` (restituisce `DATETIME`), `TIME()` (restituisce `TIME`).
'    - Da Stringhe: `DATEVALUE("yyyy-MM-dd")` converte una stringa in un `DATE`.
'    - Da Componenti Numeriche: `DATESERIAL(anno, mese, giorno)` e `TIMESERIAL(ora, min, sec)` costruiscono
'      valori `DATE` e `TIME` a partire dai loro componenti interi.
'
' 3. ARITMETICA TEMPORALE
'    - `DATE + TIME` -> `DATETIME`: Combinazione di data e ora.
'    - `DATE/DATETIME + numero` -> `DATE/DATETIME`: Aggiunge un numero di giorni (anche frazionari per `DATETIME`).
'    - `DATE/DATETIME - DATE/DATETIME` -> `TIMESPAN`: Calcola l'intervallo tra due momenti.
'    - `DATE/DATETIME + TIMESPAN` -> `DATE/DATETIME`: Aggiunge una durata a una data.
'
' 4. FUNZIONI DI MANIPOLAZIONE E ANALISI (Namespace DATETIME)
'    - `DATEADD(intervallo$, numero, data)`: Aggiunge un intervallo specifico (es. "m" per mesi, "h" per ore) a una data.
'    - `DATEDIFF(intervallo$, data1, data2)`: Calcola la differenza tra due date in un'unità specifica.
'    - `YEAR(data)`, `MONTH(data)`, `DAY(data)`, `HOUR(data)`, etc.: Estraggono i singoli componenti da un valore
'      `DATETIME`.
'    - `WEEKDAY(data)`: Restituisce il giorno della settimana.
' ======================================================================================================================

' Importa il namespace DATETIME per accedere alle funzioni di manipolazione.
IMPORT DATETIME

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ TypesDateTime - Tipi Data/Ora                                           ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
20 PRINT "--- 1. Dichiarazione, Default e Assegnazioni ---"
' Dichiarazione multipla di variabili con tipi data/ora
30 DIM D AS DATE, T AS TIME, DT AS DATETIME, TS AS TIMESPAN
40 PRINT "Default DATE:"; D
50 PRINT "Default TIME:"; T
60 PRINT
' Creazione di valori di tipo DATE e TIME
70 D = DATEVALUE("2025-10-31")
80 T = TIMESERIAL(22, 15, 30)
90 PRINT "Data Assegnata:"; D
100 PRINT "Ora Assegnata:"; T
' Combinazione di DATE e TIME per creare un DATETIME
110 DT = D + T
120 PRINT "DATETIME Combinato:"; DT
130 PRINT

140 PRINT "--- 2. Aritmetica su Date e TimeSpan ---"
150 DIM D2 AS DATE
' Aggiunta di un numero a una data (interpreta il numero come giorni)
160 D2 = D + 10
170 PRINT D; " + 10 giorni ="; D2
180
' Sottrazione tra due date per ottenere un TimeSpan
190 TS = D2 - D
200 PRINT D2; " - "; D; " = "; TS; " (TimeSpan)"
210
' Aggiunta di un numero frazionario a un DATETIME
220 DT = DT + 1.5 ' Aggiunge 1 giorno e 12 ore
230 PRINT "Nuovo DATETIME:"; DT
240 PRINT

250 PRINT "--- 3. Funzioni di Estrazione (YEAR, MONTH, etc.) ---"
' Estrazione dei componenti da un DATETIME
260 PRINT "YEAR(" ; DT; ") = "; YEAR(DT)
270 PRINT "MONTH(" ; DT; ") = "; MONTH(DT)
280 PRINT "DAY(" ; DT; ") = "; DAY(DT)
290 PRINT "HOUR(" ; DT; ") = "; HOUR(DT)
300 PRINT "MINUTE(" ; DT; ") = "; MINUTE(DT)
310 PRINT "SECOND(" ; DT; ") = "; SECOND(DT)
320 PRINT "WEEKDAY(" ; DT; ") = "; WEEKDAY(DT); "(Dom=1, Lun=2...)"
330 PRINT

340 PRINT "--- 4. Funzioni di Creazione e di Sistema ---"
350 PRINT "DATESERIAL(2026, 1, 1) ="; DATESERIAL(2026, 1, 1)
360 PRINT "TIMESERIAL(10, 20, 30) ="; TIMESERIAL(10, 20, 30)
' Funzioni che restituiscono i valori correnti del sistema
370 PRINT "NOW() ="; NOW()
380 PRINT "TODAY() ="; TODAY()
390 PRINT "TIME() ="; TIME()
400 PRINT "STRTIME() ="; STRTIME()
410 PRINT

420 PRINT "--- 5. Funzioni DATEDIFF e DATEADD ---"
430 DIM DATA_INIZIO AS DATE, DATA_FINE AS DATETIME
440 DATA_INIZIO = DATESERIAL(2025, 1, 1)
' Aggiunge 3 mesi a una data usando DATEADD per una manipolazione precisa
450 DATA_FINE = DATEADD("m", 3, DATA_INIZIO)
460 PRINT "3 mesi dopo il "; DATA_INIZIO; " è il "; DATA_FINE
' Calcola la differenza in giorni usando DATEDIFF
470 GIORNI_PASSATI = DATEDIFF("d", DATA_INIZIO, DATA_FINE)
480 PRINT "Differenza (TimeSpan):"; GIORNI_PASSATI
490 PRINT

500 PRINT "--- 6. Funzioni data estese ---"
510 PRINT "DAYSINMONTH(";D;")="; DAYSINMONTH(D)
520 PRINT "ISLEAPYEAR(2024)="; ISLEAPYEAR(2024)
530 PRINT "ISLEAPYEAR(2023)="; ISLEAPYEAR(2023)
540 PRINT "DATEFROMUNIX(1704067200)>0="; DATEFROMUNIX(1704067200) <> 0
550 PRINT "UNIXTIMESTAMP(";D;")>0="; UNIXTIMESTAMP(D) > 0
560 PRINT "FIRSTDAYOFMONTH(";D;") DAY="; DAY(FIRSTDAYOFMONTH(D))
570 PRINT "EOMONTH(";D;",0) DAY>27="; DAY(EOMONTH(D,0)) >= 28
580 PRINT "DTOS(";D;")="; DTOS(D)
590 PRINT "ISDATE('2026-01-15')="; ISDATE("2026-01-15")
600 PRINT "ISDATE('xyz')="; ISDATE("xyz")
610 PRINT "ISNUMERIC('123.45')="; ISNUMERIC("123.45")
620 PRINT "ISNUMERIC('abc')="; ISNUMERIC("abc")
625 PRINT
' --- 7. FORMAT: maschera NUMERICA vs maschera DATA sullo stesso valore ---
' Una maschera con '#'/'0' e' NUMERICA: su una data si formatta il seriale OADate
' (come le gambe C). Senza token numerici la maschera e' di data/ora.
626 PRINT "--- 7. FORMAT su una data ---"
627 PRINT "FORMAT(D, '0.000000') =", FORMAT(D, "0.000000"); "   (seriale OADate)"
628 PRINT "FORMAT(D, 'dd/MM/yyyy') =", FORMAT(D, "dd/MM/yyyy"); "   (data leggibile)"
629 PRINT "FORMAT(T, '0.000000') =", FORMAT(T, "0.000000"); "   (TIME = frazione di giornata)"
630 PRINT "FORMAT(T, 'HH:mm:ss') =", FORMAT(T, "HH:mm:ss")
631 PRINT

640 PRINT "--- Test completato ---"
650 END
