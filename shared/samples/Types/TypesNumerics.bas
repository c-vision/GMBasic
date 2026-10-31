' FILE: Types/TypesNumerics.bas
' ======================================================================================================================
' GUIDA TEORICA: TIPI DI DATO INTERI E LOGICI
' ======================================================================================================================
' Questo programma testa i tipi di dato numerici interi e booleani di GM-BASIC, insieme agli operatori
' e alle funzioni associate.
'
' 1. TIPI INTERI
'    - `BYTE`: Intero senza segno a 8-bit (valori da 0 a 255).
'    - `INTEGER`: Intero con segno a 16-bit (da -32,768 a 32,767).
'    - `LONG`: Intero con segno a 32-bit.
'    - `QUAD`: Intero con segno a 64-bit (non testato qui, ma disponibile).
'    - `BIT`: Tipo booleano. Memorizza `0` (FALSE) o `-1` (TRUE).
'
' 2. CONTROLLO DEGLI ERRORI (`ON ERROR`)
'    - `ON ERROR GOTO <label>`: Imposta un gestore di errori. Se si verifica un errore di runtime (come un
'      overflow numerico), l'esecuzione salta immediatamente all'etichetta specificata.
'    - `RESUME <label>`: All'interno del gestore di errori, `RESUME` permette di riprendere l'esecuzione
'      da un punto specifico del programma (in questo caso, dopo l'istruzione che ha causato l'errore).
'    - `ON ERROR GOTO 0`: Disabilita la gestione degli errori.
'    - `ERR.Number`: Una variabile di sistema che contiene il codice numerico dell'ultimo errore.
'
' 3. OPERATORI SPECIFICI
'    - `\`: Divisione intera. Restituisce la parte intera della divisione. Esempio: `10 \ 3` = `3`.
'    - `MOD`: Operatore modulo. Restituisce il resto della divisione. Esempio: `10 MOD 3` = `1`.
'    - `NOT`: negazione LOGICA (`NOT 0` = -1, `NOT 5` = 0; il bitwise puro è `!`).
'      `AND`/`OR`: LOGICI con short-circuit e risultato "valore" (`6 AND 3` = 3,
'      `6 OR 0` = -1, `0 OR 5` = 5) — canone in Guide/utente/XOR-AND-OR-SWAP-Guida-Utente.md.
'
' 4. PROMOZIONE DEI TIPI
'    - Quando un'operazione coinvolge tipi interi di dimensioni diverse, il risultato viene promosso al tipo
'      più grande per evitare perdita di dati. Esempio: `BYTE + INTEGER` risulta in un `INTEGER`.
'
' 5. FUNZIONI DI CONVERSIONE E ARROTONDAMENTO
'    - `CINT(n)`: Converte un numero in `INTEGER`, arrotondandolo.
'    - `TRUNC(n)`: Tronca un numero, rimuovendo la parte decimale senza arrotondare.
' ======================================================================================================================

' Importa i namespace necessari per CINT e TRUNC
IMPORT CONVERSIONS
IMPORT MATH

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ TypesNumerics - Tipi Numerici (INTEGER, LONG, etc)                      ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
20 PRINT "--- 1. Dichiarazione e Valori di Default ---"
' Dichiarazione multipla su una riga
30 DIM I AS INTEGER : DIM L AS LONG : DIM B AS BYTE : DIM FLAG AS BIT
40 PRINT "Default INTEGER:"; I; "(Atteso 0)"
50 PRINT "Default LONG:"; L; "(Atteso 0)"
60 PRINT "Default BYTE:"; B; "(Atteso 0)"
70 PRINT "Default BIT:"; FLAG; "(Atteso False/0)"
80 PRINT

90 PRINT "--- 2. Assegnazioni e Gestione Errori (ON ERROR) ---"
100 B = 255
110 PRINT "BYTE assegnato a 255: OK. Valore:"; B
120 I = 32767
130 PRINT "INTEGER assegnato a 32767: OK. Valore:"; I
140 PRINT "Ora sollevo un errore a RUNTIME (verrà gestito da ON ERROR)..."
' Imposta un gestore di errori
150 ON ERROR GOTO 1000
' `ERROR n` solleva l'errore n a RUNTIME (canone condiviso dalle 4 gambe). NOTA: un
' overflow di narrowing su un LITERALE (es. `B = 256` con BYTE) è diagnosticato in
' COMPILAZIONE (ErrorCode.Overflow), non a runtime; qui serve un errore eseguibile.
160 ERROR 7
' Se l'errore non viene gestito, questa riga viene stampata
170 PRINT "ERRORE: Questa riga non doveva essere eseguita."
' Salta il gestore di errori se non si verifica l'errore
180 GOTO 200

' Blocco del gestore di errori
1000 PRINT "OK: Errore 7 sollevato e intercettato (ERR="; ERR.Number; ")." : RESUME 200

' Riprende l'esecuzione qui dopo l'errore
200 ON ERROR GOTO 0 ' Disattiva il gestore di errori
210 PRINT

220 PRINT "--- 3. Test Operatori \ e MOD ---"
230 DIM RES_I AS INTEGER : DIM RES_L AS LONG
' Divisione intera
240 RES_I = 10 \ 3
250 PRINT "10 \ 3 (Integer) ="; RES_I; "(Atteso 3)"
260 L = 100
' Operatore modulo
270 RES_L = L MOD 30
280 PRINT "100 MOD 30 (Long) ="; RES_L; "(Atteso 10)"
290 PRINT

300 PRINT "--- 4. Promozione dei Tipi in Espressioni ---"
310 B = 100
320 I = 1000
330 L = 100000
340 D = 0.5
350 
' Il risultato viene promosso al tipo più grande (INTEGER)
360 PRINT "Byte + Integer ="; B + I
' Il risultato viene promosso a LONG
370 PRINT "Integer + Long ="; I + L
' Il risultato viene promosso a DOUBLE
380 PRINT "Long + Double ="; L + D
' La divisione standard è sempre a virgola mobile
390 PRINT "10 / 3 (divisione float) ="; 10 / 3
400 PRINT

410 PRINT "--- 5. Test Tipo BIT e Conversioni ---"
420 DIM VERO AS BIT : DIM FALSO AS BIT
' In GM-BASIC, qualsiasi valore diverso da zero è TRUE, ma la rappresentazione canonica è -1.
430 VERO = -1
440 FALSO = 0
450 PRINT "VERO assegnato da -1: "; VERO
460 PRINT "FALSO assegnato da 0: "; FALSO
' Il tipo BIT può essere usato in espressioni numeriche.
470 PRINT "Valore numerico di VERO:"; VERO + 0; "(Atteso -1)"
480 PRINT "Valore numerico di FALSO:"; FALSO + 0; "(Atteso 0)"
490 
' Test delle funzioni di conversione a intero
500 PRINT "CINT(123.7) ="; CINT(123.7); "(Atteso 124, arrotondamento)"
505 PRINT "TRUNC(123.7) ="; TRUNC(123.7); "(Atteso 123, troncamento)"
510 PRINT

520 PRINT "--- 6. Test Operatori (NOT logico; AND/OR logici con short-circuit; ! bitwise) ---"
' Operazioni logiche sui bit dei numeri
530 PRINT "6 (0110) AND 3 (0011) ="; 6 AND 3; "(Atteso 3: sinistra vera -> valore destro)"
540 PRINT "6 (0110) OR 3 (0011) ="; 6 OR 3; "(Atteso -1: sinistra vera -> TRUE)"
' NOT è un'inversione bit a bit (complemento a due)
550 PRINT "NOT 5 ="; NOT 5; "(Atteso 0: NOT logico; il bitwise e'' !)"
560 PRINT "VERO (-1) AND 3 ="; VERO AND 3; "(Atteso 3)"
570 PRINT "FALSO (0) OR 5 ="; FALSO OR 5; "(Atteso 5)"
580 PRINT

590 PRINT "--- Test completato ---"
600 END
