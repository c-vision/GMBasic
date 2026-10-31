' FILE: Types/TypesFixedLengthStrings.bas
' ======================================================================================================================
' GUIDA TEORICA: STRINGHE A LUNGHEZZA FISSA
' ======================================================================================================================
' GM-BASIC supporta le stringhe a lunghezza fissa, un costrutto potente ereditato dai dialetti BASIC classici,
' particolarmente utile per la manipolazione di record a layout fisso (tipici dei file di database) e per
' l'interfacciamento con API a basso livello.
'
' 1. SINTASSI E DICHIARAZIONE
'    - `DIM nomeVar AS STRING * lunghezza`
'    - `lunghezza`: Un'espressione costante intera che definisce la dimensione esatta della stringa.
'
' 2. COMPORTAMENTO IN ASSEGNAZIONE
'    - Padding (Riempimento): Se alla variabile viene assegnata una stringa più corta della sua lunghezza fissa,
'      la stringa viene riempita con spazi a destra fino a raggiungere la lunghezza definita.
'      Esempio: `CODICE = "ABC"` -> `CODICE` conterrà `"ABC       "`.
'    - Truncation (Troncamento): Se viene assegnata una stringa più lunga, viene troncata alla lunghezza fissa.
'      Esempio: `CODICE = "1234567890XYZ"` -> `CODICE` conterrà `"1234567890"`.
'
' 3. UTILIZZO IN FUNZIONI E SUBROUTINE
'    - Tipo di Ritorno: Una funzione può essere dichiarata per restituire una stringa a lunghezza fissa
'      (`FUNCTION Nome(...) AS STRING * N`). Qualsiasi valore restituito verrà automaticamente paddato o
'      troncato.
'    - Parametri: Le subroutine possono accettare parametri a lunghezza fissa, sia per valore (`BYVAL`) che
'      per riferimento (`BYREF`).
'      - `BYVAL`: Una copia del valore viene passata, già formattata (paddata/troncata).
'      - `BYREF`: Viene passato un riferimento alla variabile originale. Le modifiche al parametro
'        all'interno della SUB si riflettono sulla variabile del chiamante.
'
' 4. COERCIZIONE DI TIPO
'    - Quando un valore non-stringa (es. un numero) viene assegnato a una variabile stringa (sia a
'      lunghezza fissa che variabile), viene implicitamente convertito in stringa, come se fosse
'      stato usato `CSTR()`, prima di applicare le regole di padding/troncamento.
' ======================================================================================================================

' Funzione che restituisce una stringa a lunghezza fissa di 8 caratteri.
FUNCTION FormattaID(ID) AS STRING * 8
  ' Il numero 'ID' viene prima convertito in stringa, poi paddato a 8 caratteri.
  RETURN ID
END FUNCTION

' Subroutine che accetta una stringa a lunghezza variabile (BYVAL)
' e una a lunghezza fissa (BYREF).
SUB ProvaTipi(BYVAL Messaggio AS STRING, BYREF MessaggioRef AS STRING * 10)
  PRINT "Dentro la SUB:"
  ' 'Messaggio' è una copia locale.
  PRINT "  Messaggio (BYVAL): '"; Messaggio; "'"
  ' 'MessaggioRef' è un riferimento alla variabile originale.
  PRINT "  MessaggioRef (BYREF) prima: '"; MessaggioRef; "'"
  ' Questa assegnazione modifica la variabile del chiamante.
  MessaggioRef = "CAMBIATO"
  PRINT "  MessaggioRef (BYREF) dopo: '"; MessaggioRef; "'"
END SUB

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ TypesFixedLengthStrings - Tipi Espliciti (AS STRING)                    ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
20 PRINT "--- 1. Test Variabili Scalari ---"
' Stringa a lunghezza variabile
30 DIM NOME AS STRING
' Stringa a lunghezza fissa
40 DIM CODICE AS STRING * 10
50
60 NOME = "MARIO ROSSI"
70 PRINT "NOME (variabile): '"; NOME; "'"
80
90 PRINT "Assegno 'ABC' a CODICE (lunghezza fissa 10)..."
100 CODICE = "ABC"
110 PRINT "CODICE (fisso): '"; CODICE; "' (Atteso padding a destra)"
120 PRINT
130 PRINT "Assegno '1234567890XYZ' a CODICE..."
140 CODICE = "1234567890XYZ"
150 PRINT "CODICE (fisso): '"; CODICE; "' (Atteso troncamento)"
160 PRINT

180 PRINT "--- 2. Test Ritorno Funzione a Lunghezza Fissa ---"
' La funzione FormattaID restituisce STRING * 8. L'assegnazione a una stringa a lunghezza
' variabile (ID_FORMATTATO$) preserva il risultato formattato.
190 ID_FORMATTATO$ = FormattaID(987)
200 PRINT "ID Formattato: '"; ID_FORMATTATO$; "' (Atteso coercizione e padding a 8 caratteri)"
210 PRINT

230 PRINT "--- 3. Test Parametri Tipizzati in SUB (BYVAL/BYREF) ---"
240 DIM MESSAGGIO_ORIGINALE AS STRING * 10
250 MESSAGGIO_ORIGINALE = "ORIGINALE"
260 PRINT "Variabile prima della chiamata: '"; MESSAGGIO_ORIGINALE; "'"
' Passa una stringa letterale (BYVAL) e un riferimento a MESSAGGIO_ORIGINALE (BYREF)
270 ProvaTipi("Valore passato", MESSAGGIO_ORIGINALE)
' La modifica fatta in ProvaTipi su MessaggioRef sarà visibile qui.
280 PRINT "Variabile dopo la chiamata: '"; MESSAGGIO_ORIGINALE; "' (Atteso: 'CAMBIATO' + padding)"
290 PRINT

310 PRINT "--- Test completato. ---"
320 END
