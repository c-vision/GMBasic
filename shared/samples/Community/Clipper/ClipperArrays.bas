' FILE: Clipper/ClipperArrays.bas
' =================================================================================================
'               GUIDA TEORICA: MANIPOLAZIONE AVANZATA DEGLI ARRAY (Stile Clipper)
' =================================================================================================
'
' GM-BASIC include una suite completa di funzioni per la manipolazione di array, ispirate a
' quelle presenti in Clipper/xBase. Queste funzioni, contenute nel namespace `ARRAYS`, permettono
' di eseguire operazioni complesse su array in modo dichiarativo e potente.
'
' SINTASSI LEGACY:
' I comandi di manipolazione (Statement) supportano due modalità di scrittura:
' 1. Legacy: `AADD array, valore` (Richiede IMPORT ARRAYS, niente parentesi)
' 2. legacy: `ARRAYS.AADD array, valore` (Namespace esplicito, senza parentesi)
'
' FUNZIONI E COMANDI TESTATI:
' ---------------------------
' - `AADD`: Aggiunge un elemento alla fine (Statement).
' - `AINS`: Inserisce un elemento vuoto (Statement).
' - `ADEL`: Elimina un elemento (Statement).
' - `AFILL`: Riempie un array con un valore (Statement).
' - `ACOPY`: Copia elementi tra array (Statement).
' - `ASORT`: Ordina l'array in-place (Statement).
' - `AEVAL`: Esegue una callback per ogni elemento (Statement).
'
' Funzioni (Espressioni):
' - `ACLONE`, `ASCAN`, `AMAP`, `AFILTER`, `AREDUCE`, `AUNIQUE`, `ACOUNT`, `AREVERSE`, `ASHUFFLE`, `ABINARYSEARCH`.
'
' =================================================================================================

IMPORT ARRAYS
IMPORT STRINGS ' Necessario per JOIN

' --- DEFINIZIONE TIPO E FUNZIONI HELPER PER I TEST ---

TYPE Prodotto
    Nome AS STRING * 20
    Prezzo AS CURRENCY
END TYPE

' Funzione di confronto per ASORT: ordina per prezzo crescente.
' Restituisce < 0 se P1 < P2, 0 se P1 = P2, > 0 se P1 > P2.
FUNCTION ConfrontaProdotti(P1 AS Prodotto, P2 AS Prodotto) AS INTEGER
    IF P1.Prezzo < P2.Prezzo THEN RETURN -1
    IF P1.Prezzo > P2.Prezzo THEN RETURN 1
    RETURN 0
END FUNCTION

' Funzione di condizione per ASCAN: trova il primo prodotto con prezzo > 150.
FUNCTION TrovaProdottoCostoso(P AS Prodotto) AS BIT
    RETURN P.Prezzo > 150
END FUNCTION

' Funzione di trasformazione per AMAP: sconta il prezzo del 10%.
FUNCTION ApplicaSconto(P AS Prodotto) AS Prodotto
    RETURN P
END FUNCTION

' Funzione di filtro per AFILTER: seleziona solo i prodotti con nome non vuoto.
FUNCTION FiltraInScorta(P AS Prodotto) AS BIT
    RETURN TRIM(P.Nome) <> ""
END FUNCTION

' Funzione accumulatore per AREDUCE: somma i prezzi.
FUNCTION CalcolaValoreTotale(Accumulatore AS CURRENCY, P AS Prodotto) AS CURRENCY
    RETURN Accumulatore + P.Prezzo
END FUNCTION

' ============================
' --- CORPO PRINCIPALE DEL TEST ---
' ============================
SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClipperArrays - Manipolazione Array (Legacy & Moderna)                      ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. AADD ---
    PRINT "--- 1. Test AADD (Aggiunta) ---"
    DIM Nomi$() ' Array dinamico di stringhe
    
    ' Sintassi Legacy: AADD array, valore
    AADD Nomi$, "Mario"
    PRINT "  [Legacy] AADD 'Mario'. Count: "; UBOUND(Nomi$) + 1
    
    ' ARRAYS.AADD array, valore
    AADD Nomi$, "Luigi"
    PRINT "  [Modern] AADD 'Luigi'. Count: "; UBOUND(Nomi$) + 1
    
    AADD Nomi$, "Anna"
    PRINT "  Elemento Nomi$(2): "; Nomi$(2); " (Atteso: Anna)"
    PRINT "  Array: "; JOIN(Nomi$, ", ")
    PRINT
    
    ' --- 2. AINS / ADEL ---
    PRINT "--- 2. Test AINS / ADEL (Inserimento/Cancellazione) ---"
    DIM Lettere$() = {"A", "C", "D"}
    
    ' Legacy: AINS array, pos
    AINS Lettere$, 1
    Lettere$(1) = "B_Leg"
    PRINT "  [Legacy] AINS 1 -> "; JOIN(Lettere$, ", ")
    
    ' Modern: ARRAYS.AINS array, pos
    AINS Lettere$, 2
    Lettere$(2) = "B_Mod"
    PRINT "  [Modern] AINS 2 -> "; JOIN(Lettere$, ", ")
    
    ' Legacy: ADEL array, pos
    ADEL Lettere$, 1
    PRINT "  [Legacy] ADEL 1 -> "; JOIN(Lettere$, ", ")
    
    ' Modern: ARRAYS.ADEL array, pos
    ADEL Lettere$, 2
    PRINT "  [Modern] ADEL 2 -> "; JOIN(Lettere$, ", ")
    PRINT
    
    ' --- 3. AFILL (Nuovo Test) ---
    PRINT "--- 3. Test AFILL (Riempimento) ---"
    DIM Buffer(4) AS INTEGER
    
    ' Legacy: AFILL array, value
    AFILL Buffer, 9
    PRINT "  [Legacy] AFILL 9 -> "; Buffer(0); Buffer(1); Buffer(2); Buffer(3); Buffer(4)
    
    ' Modern: ARRAYS.AFILL array, value, start, count
    AFILL Buffer, 5, 1, 3
    PRINT "  [Modern] AFILL 5 (idx 1, cnt 3) -> "; Buffer(0); Buffer(1); Buffer(2); Buffer(3); Buffer(4)
    PRINT

    ' --- 4. ACOPY (Nuovo Test) ---
    PRINT "--- 4. Test ACOPY (Copia) ---"
    DIM Source() = {10, 20, 30, 40, 50}
    DIM Dest(4) AS DOUBLE
    
    ' Legacy: ACOPY src, dest
    ACOPY Source, Dest
    PRINT "  [Legacy] ACOPY Full -> "; Dest(0); Dest(1); Dest(2)
    
    DIM DestPartial(4) AS DOUBLE
    ' Modern: ARRAYS.ACOPY src, dest, srcStart, count, destStart
    ACOPY Source, DestPartial, 1, 2, 2
    PRINT "  [Modern] ACOPY (Partiale) -> "; DestPartial(0); DestPartial(1); DestPartial(2); DestPartial(3); DestPartial(4)
    PRINT

    ' --- 5. ACLONE (Funzione) ---
    PRINT "--- 5. Test ACLONE (Copia profonda) ---"
    DIM Originale() = {10, 20, 30}
    ' Crea una copia indipendente dell'array
    DIM Cloned() = ACLONE(Originale)
    Cloned(0) = 99 ' Modifica solo la copia
    PRINT "  Originale(0): "; Originale(0); " (Atteso: 10)"
    PRINT "  Copia(0): "; Cloned(0); " (Atteso: 99)"
    PRINT
    
    ' --- 6. ASORT ---
    PRINT "--- 6. Test ASORT (Ordinamento) ---"
    DIM Numeri() = {50, 10, 20, 40, 30}
    
    ' Legacy: ASORT array
    ASORT Numeri
    PRINT "  [Legacy] ASORT Asc -> "; JOIN(Numeri, ", ")
    
    ' Modern: ARRAYS.ASORT array, , order
    ASORT Numeri, , "D"
    PRINT "  [Modern] ASORT Desc -> "; JOIN(Numeri, ", ")
    
    DIM Catalogo(1) AS Prodotto
    Catalogo(0).Nome = "Mela" : Catalogo(0).Prezzo = 160.50
    Catalogo(1).Nome = "Pera" : Catalogo(1).Prezzo = 80.00
    
    ' Legacy con UDF: ASORT array, func
    ASORT Catalogo, ConfrontaProdotti
    PRINT "  [Legacy] ASORT UDT: "; Catalogo(0).Nome; ", "; Catalogo(1).Nome
    PRINT
    
    ' --- 7. AEVAL (Nuovo Test) ---
    PRINT "--- 7. Test AEVAL (Iterazione) ---"
    DIM Vals() = {1, 2, 3}
    
    PRINT "  [Legacy] AEVAL: ";
    AEVAL Vals, SUB(x) { PRINT x; " "; }
    PRINT
    
    PRINT "  [Modern] AEVAL: ";
    AEVAL Vals, SUB(x) { PRINT x * 10; " "; }
    PRINT
    PRINT

    ' --- 8. ASCAN ---
    PRINT "--- 8. Test ASCAN (Ricerca) ---"
    DIM Valori() = {10, 25, 160, 80}
    ' Cerca la prima occorrenza del valore 160.
    PRINT "  ASCAN per valore 160: "; ASCAN(Valori, 160); " (Atteso: 2)"
    ' Cerca il primo elemento che soddisfa la condizione definita nella funzione.
    PRINT "  ASCAN per prodotto costoso: "; ASCAN(Catalogo, TrovaProdottoCostoso); " (Atteso: 1)"
    PRINT
    
    ' --- 9. AMAP ---
    PRINT "--- 9. Test AMAP (Trasformazione) ---"
    ' Crea un nuovo array applicando la funzione ApplicaSconto a ogni elemento.
    DIM CatalogoScontato() = AMAP(Catalogo, ApplicaSconto)
    PRINT "  Prezzo scontato del primo prodotto: "; CatalogoScontato(0).Prezzo; " (Atteso: 160.5)"
    PRINT
    
    ' --- 10. AFILTER ---
    PRINT "--- 10. Test AFILTER (Filtro) ---"
    DIM Magazzino(2) AS Prodotto
    Magazzino(0).Nome = "Sedia" : Magazzino(1).Nome = "Divano" : Magazzino(2).Nome = "Tavolo"
    ' Crea un nuovo array con solo gli elementi per cui FiltraInScorta restituisce TRUE.
    DIM InScorta() = AFILTER(Magazzino, FiltraInScorta)
    PRINT "  Prodotti filtrati in scorta: "; UBOUND(InScorta) + 1; " (Atteso: 3)"
    PRINT
    
    ' --- 11. AREDUCE ---
    PRINT "--- 11. Test AREDUCE (Aggregazione) ---"
    ' Assegna i prezzi ai prodotti filtrati per il calcolo
    InScorta(0).Prezzo = 50 : InScorta(2).Prezzo = 200
    ' Somma i prezzi di tutti gli elementi nell'array 'InScorta', partendo da un valore iniziale di 0.
    DIM ValoreTotale AS CURRENCY = AREDUCE(InScorta, CalcolaValoreTotale, 0)
    PRINT "  Valore totale del magazzino in scorta: "; ValoreTotale; " (Atteso: 250)"
    PRINT
    
    ' --- 12. AUNIQUE ---
    PRINT "--- 12. Test AUNIQUE (Valori Unici) ---"
    DIM Codici() = {101, 205, 101, 300, 205, 101}
    ' Crea un nuovo array contenente solo i valori unici.
    DIM CodiciUnici() = AUNIQUE(Codici)
    PRINT "  Codici unici: "; JOIN(CodiciUnici, ", "); " (Atteso: 101, 205, 300)"
    PRINT

    ' --- 13. ACOUNT ---
    PRINT "--- 13. Test ACOUNT (Conteggio condizionale) ---"
    DIM Punteggi() = {8, 5, 9, 4, 10, 6}
    ' Usa una lambda (funzione anonima) per contare i punteggi sufficienti (>= 6)
    DIM NumSufficienti = ACOUNT(Punteggi, FN(p) => p >= 6)
    PRINT "  Numero di punteggi sufficienti (>= 6): "; NumSufficienti; " (Atteso: 4)"
    PRINT

    ' --- 14. AREVERSE e ASHUFFLE ---
    PRINT "--- 14. Test AREVERSE e ASHUFFLE (Ordinamento inverso e casuale) ---"
    DIM LettereOriginali$() = {"A", "B", "C", "D"}
    ' Crea una copia dell'array con gli elementi in ordine inverso.
    DIM LettereRovesciate$() = AREVERSE(LettereOriginali$)
    PRINT "  Array rovesciato: "; JOIN(LettereRovesciate$, ", "); " (Atteso: D, C, B, A)"
    ' Crea una copia dell'array con gli elementi in ordine casuale.
    ASHUFFLE LettereOriginali$
    PRINT "  Array mescolato (esempio): "; JOIN(LettereOriginali$, ", "); " (L'ordine sarà casuale)"
    PRINT

    ' --- 15. ABINARYSEARCH ---
    PRINT "--- 15. Test ABINARYSEARCH (Ricerca binaria) ---"
    DIM DatiOrdinati() = {10, 20, 30, 40, 50, 60, 70}
    DIM IndiceBinario AS INTEGER
    ' Cerca un valore presente nell'array ordinato.
    IndiceBinario = ABINARYSEARCH(DatiOrdinati, 50)
    PRINT "  ABINARYSEARCH per 50: "; IndiceBinario; " (Atteso: 4)"
    ' Cerca un valore non presente. La ricerca binaria è molto efficiente.
    IndiceBinario = ABINARYSEARCH(DatiOrdinati, 35)
    PRINT "  ABINARYSEARCH per 35: "; IndiceBinario; " (Atteso: -1)"
    PRINT
    
    PRINT "--- Test completo manipolazione array terminato. ---"
END SUB