' FILE: Exceptions/ExceptionsTryIfSelect.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE ERRORI STRUTTURATA (TRY...CATCH)
' =================================================================================================
'
' Questo test verifica l'implementazione della gestione strutturata delle eccezioni in GM-BASIC,
' che offre un approccio più robusto e moderno rispetto a `ON ERROR GOTO`.
'
' 1. IL BLOCCO `TRY...END TRY`
'    Definisce una sezione di codice "protetta". Se si verifica un errore (eccezione) all'interno
'    di questo blocco, il flusso di esecuzione viene interrotto e trasferito al primo blocco `CATCH`
'    compatibile.
'
' 2. CLAUSOLE `CATCH`
'    - Sintassi: `CATCH variabile AS Tipo [WHEN condizione]`
'    - GM-BASIC supporta catture multiple. L'ordine è importante: vengono valutate dall'alto verso il basso.
'    - `variabile`: Riceve l'oggetto errore (tipo `ERR`). Questo oggetto è locale al blocco CATCH.
'    - `WHEN condizione`: Un filtro aggiuntivo. Il blocco viene eseguito solo se la condizione è vera.
'      Questo permette di gestire codici di errore specifici senza dover nidificare IF.
'
' 3. CLAUSOLA `FINALLY`
'    - Contiene codice che viene eseguito *sempre*, sia che si verifichi un errore, sia che il
'      codice nel `TRY` termini normalmente.
'    - È fondamentale per il rilascio delle risorse (chiusura file, connessioni, etc.).
'
' 4. INTERAZIONE CON IL FLUSSO DI CONTROLLO
'    Questo test verifica specificamente che le strutture di controllo standard (`SELECT CASE`, `IF`)
'    funzionino correttamente all'interno di un blocco `TRY` e che l'istruzione `ERROR` (che lancia
'    manualmente un'eccezione) scateni correttamente il meccanismo di cattura.
'
' =================================================================================================

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ExceptionsTryIfSelect - Test AST per TRY, IF, SELECT                     ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT

DIM N
N = 4  ' Default: nessun errore (cambiare per testare altri casi)
' INPUT "Inserisci un numero (1=Errore A, 2=Errore B, 3=Altro Errore, 4=Nessun Errore): "; N

' Inizio del blocco di gestione errori strutturata.
20 TRY
30   PRINT "Dentro il blocco TRY..."
40   
     ' Struttura di controllo annidata nel TRY.
50   SELECT CASE N
60     CASE 1
         ' Genera manualmente l'errore 11 (Divisione per zero simulata).
70       ERROR 11, "Errore A (Divisione per zero)"
80     CASE 2
         ' Genera manualmente l'errore 53 (File non trovato simulato).
90       ERROR 53, "Errore B (File non trovato)"
100    CASE 3
         ' Genera un errore generico.
110      ERROR 100, "Altro errore"
120    CASE ELSE
         ' Nessun errore generato, il flusso prosegue normalmente.
130      PRINT "Nessun errore generato nel TRY."
140   END SELECT
150
     ' Test di IF a riga singola dentro il TRY.
160   IF N = 4 THEN PRINT "IF a riga singola eseguito."
     ' Test di salto (GOTO) fuori dal TRY (dovrebbe invocare FINALLY prima di saltare).
170   IF N > 10 GOTO fine
180
     ' Gestore specifico per l'errore 11 usando il filtro WHEN.
190 CATCH ex AS ERR WHEN ex.Number = 11
200   PRINT "Catturato Errore A (Divisione per zero). Desc: "; ex.Description
     ' Gestore specifico per l'errore 53.
210 CATCH ex AS ERR WHEN ex.Number = 53
220   PRINT "Catturato Errore B (File non trovato). Desc: "; ex.Description
     ' Gestore generico per tutti gli altri errori (fallback).
230 CATCH ex AS ERR
240   PRINT "Catturato un errore non specifico. Codice: "; ex.Number
     ' Blocco di pulizia, eseguito in ogni caso.
250 FINALLY
260   PRINT "Blocco FINALLY eseguito. Pulizia in corso..."
270 END TRY

280 PRINT "Programma terminato."
290 fine:
300 END