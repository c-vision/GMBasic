' FILE: Arrays/ArraysRedim.bas
' =================================================================================================
'                                GUIDA TEORICA: RIDIMENSIONAMENTO ARRAY (REDIM)
' =================================================================================================
'
' In GM-BASIC, gli array dichiarati senza dimensioni fisse o con dimensioni iniziali possono essere
' ridimensionati a runtime usando l'istruzione `REDIM`.
'
' 1. REDIM (Distruttivo)
'    Sintassi: `REDIM NomeArray(NuoveDimensioni)`
'    Comportamento: Crea un nuovo array con le nuove dimensioni. I dati contenuti nel vecchio array
'    vengono persi. I nuovi elementi sono inizializzati al valore di default (0, "", False).
'
' 2. REDIM PRESERVE (Conservativo)
'    Sintassi: `REDIM PRESERVE NomeArray(NuoveDimensioni)`
'    Comportamento: Ridimensiona l'array cercando di preservare i dati esistenti.
'    - Se l'array viene ingrandito, i vecchi dati sono mantenuti e i nuovi elementi sono inizializzati a default.
'    - Se l'array viene ridotto, i dati che eccedono le nuove dimensioni vengono troncati.
'    - Nota: In array multidimensionali, `PRESERVE` permette di cambiare solo l'ultima dimensione.
'
' 3. SCOPE E RIFERIMENTI
'    `REDIM` opera sulla variabile array esistente. Se l'array è stato passato a una procedura `BYREF`,
'    il `REDIM` all'interno della procedura ridimensiona l'array originale del chiamante.
'
' 4. NUOVA SINTASSI: REDIM CON INIZIALIZZATORE
'    - `REDIM NomeArray = { val1, val2, ... }`: Ridimensiona e sovrascrive l'array con i nuovi valori.
'    - `REDIM PRESERVE NomeArray(NuovaDim) = { valA, valB }`: Estende l'array e popola i *nuovi* elementi.
'
' =================================================================================================

IMPORT ARRAYS
IMPORT STRINGS

' SUB per testare il ridimensionamento di un array passato per riferimento.
' L'uso di BYREF (implicito per gli array, ma qui esplicito) garantisce che operiamo sull'originale.
SUB ModificaArray(BYREF Arr())
    PRINT
    PRINT "--- Dentro SUB (BYREF) ---"
    PRINT "  UBOUND iniziale in SUB:"; UBOUND(Arr)
    PRINT "  Valore iniziale Arr(0) ="; Arr(0); "(Atteso: 100)"
    
    PRINT "  Ridimensiono a (3) con PRESERVE..."
    ' Ridimensiona l'array preservando i dati esistenti (Arr(0)=100).
    REDIM PRESERVE Arr(3)
    
    ' Assegna un valore al nuovo elemento creato.
    Arr(3) = 999
    
    PRINT "  UBOUND finale in SUB:"; UBOUND(Arr)
    PRINT "  Arr(0) ="; Arr(0); "(Atteso: 100, preservato)"
    PRINT "  Arr(3) ="; Arr(3); "(Atteso: 999)"
    PRINT "--- Uscita da SUB ---"
    PRINT
END SUB

10 CLS
20 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ ArraysRedim - Ridimensionamento Array (REDIM)                            ║"
40 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
50 PRINT

60 PRINT "--- 1. REDIM semplice (distruttivo) ---"
' Dichiara array iniziale di 2 elementi (0, 1)
70 DIM A(1)
80 A(0) = 10: A(1) = 20
90 PRINT "Array A iniziale: A(0)="; A(0); ", A(1)="; A(1)

100 PRINT "Eseguo REDIM A(3)..."
' Ridimensiona a 4 elementi (0..3). Senza PRESERVE, i dati (10, 20) vengono persi.
110 REDIM A(3)
120 PRINT "Array A dopo REDIM: A(0)="; A(0); ", A(1)="; A(1); "(Atteso: 0, 0, dati persi)"

130 PRINT "Riassegno valori non-zero prima della chiamata..."
140 A(0) = 100: A(1) = 200
150 PRINT "  Nuovi valori: A(0)="; A(0); ", A(1)="; A(1)
160 PRINT "UBOUND(A) ="; UBOUND(A); "(Atteso: 3)"
170 PRINT

180 PRINT "--- 2. REDIM PRESERVE (conserva i dati) ---"
190 DIM B(2)
200 B(0)=11: B(1)=22: B(2)=33
210 PRINT "Array B iniziale: B(0)="; B(0); ", B(1)="; B(1); ", B(2)="; B(2)

220 PRINT "Eseguo REDIM PRESERVE B(4) (ingrandimento)..."
' Ingrandisce l'array. I dati esistenti vengono copiati.
230 REDIM PRESERVE B(4)
240 PRINT "B dopo ingrandimento: B(1)="; B(1); ", B(4)="; B(4); "(Atteso: 22, 0)"

250 PRINT "Eseguo REDIM PRESERVE B(1) (riduzione)..."
' Riduce l'array. I dati oltre l'indice 1 vengono persi.
260 REDIM PRESERVE B(1)
270 PRINT "B dopo riduzione: B(1)="; B(1); "(Atteso: 22)"
280 PRINT "UBOUND(B) ="; UBOUND(B); "(Atteso: 1)"
290 PRINT

300 PRINT "--- 3. REDIM su scope globale da dentro una SUB ---"
310 PRINT "Stato di A prima della chiamata: UBOUND(A) ="; UBOUND(A); ", A(0) ="; A(0)
' Chiama la SUB passando l'array A. La SUB farà un REDIM PRESERVE.
320 ModificaArray(A)

330 PRINT "Stato di A dopo la chiamata (deve essere stato modificato):"
340 PRINT "  UBOUND finale di A:"; UBOUND(A); "(Atteso: 3)"
350 PRINT "  A(0) ="; A(0); "(Atteso: 100, preservato)"
360 PRINT "  A(3) ="; A(3); "(Atteso: 999, nuovo valore)"
370 PRINT

380 PRINT "--- 4. Test REDIM con Inizializzatore (=) ---"
390 DIM C() = { 1, 2 }
400 PRINT "Array C iniziale: "; JOIN(C, ", ")
410 PRINT "Eseguo REDIM C = {100, 200, 300}..."
420 ' Ridimensionamento distruttivo con inizializzatore.
430 REDIM C = {100, 200, 300}
440 PRINT "Array C dopo REDIM con inizializzatore: "; JOIN(C, ", ")
450 PRINT "  UBOUND(C) ="; UBOUND(C); "(Atteso: 2)"
460 PRINT

470 PRINT "Eseguo REDIM PRESERVE C(5) = {98, 99}..."
480 ' Estende l'array a 6 elementi (0-5) e popola i nuovi elementi (indici 3, 4)
490 REDIM PRESERVE C(5) = {98, 99}
500 PRINT "Array C dopo REDIM PRESERVE con inizializzatore: "; JOIN(C, ", ")
510 PRINT "  C(0)="; C(0); "(Atteso: 100)"
520 PRINT "  C(3)="; C(3); "(Atteso: 98)"
530 PRINT "  C(5)="; C(5); "(Atteso: 0, non inizializzato)"
540 PRINT

550 PRINT "--- Test REDIM completato ---"
560 END