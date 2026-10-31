' FILE: Arrays/ArraysComplexProcessing.bas
' =================================================================================================
'                                GUIDA TEORICA: ELABORAZIONE FUNZIONALE DEGLI ARRAY
' =================================================================================================
'
' GM-BASIC estende il concetto tradizionale di array introducendo potenti primitive di programmazione
' funzionale per la manipolazione delle collezioni di dati. Queste funzioni permettono di operare
' sugli array in modo dichiarativo, riducendo la necessità di scrivere loop espliciti (FOR/NEXT) e
' migliorando la leggibilità e la manutenibilità del codice.
'
' --- FUNZIONI CHIAVE ---
'
' 1. AMAP (Array Map): PROIEZIONE
'    Trasforma ogni elemento di un array applicando una funzione specificata.
'    Restituisce un NUOVO array della stessa dimensione contenente i risultati.
'    Es: AMAP({1,2,3}, FN(x)=>x*2) -> {2,4,6}
'
' 2. AFILTER (Array Filter): SELEZIONE
'    Crea un NUOVO array contenente solo gli elementi dell'array originale che soddisfano
'    una condizione (predicato).
'    Es: AFILTER({1,2,3,4}, FN(x)=>x>2) -> {3,4}
'
' 3. AREDUCE (Array Reduce): AGGREGAZIONE
'    Combina tutti gli elementi di un array in un singolo valore accumulato, applicando
'    una funzione binaria.
'    Es: AREDUCE({1,2,3}, FN(acc,x)=>acc+x) -> 6
'
' 4. AEVAL (Array Evaluate): ITERAZIONE
'    Esegue una procedura (SUB) per ogni elemento dell'array. Utile per effetti collaterali (es. stampa).
'
' 5. ASCAN (Array Scan): RICERCA
'    Cerca un elemento che soddisfa una condizione e ne restituisce l'indice.
'
' 6. ASORT (Array Sort): ORDINAMENTO
'    Ordina l'array in-place, supportando funzioni di confronto personalizzate per tipi complessi (UDT).
'
' --- REQUISITI ---
' Questo esempio utilizza UDT (User Defined Types), Lambda Expressions e il namespace ARRAYS.
' =================================================================================================

IMPORT ARRAYS

// Definiamo un tipo di dato per rappresentare un singolo ordine.
// Gli UDT (User Defined Types) permettono di creare strutture dati complesse.
TYPE Vendita
    ID AS LONG
    Prodotto AS STRING * 30
    Quantita AS INTEGER
    PrezzoUnitario AS CURRENCY
    Annullato AS BOOLEAN
END TYPE

SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ArraysComplexProcessing - Elaborazione Dati con Funzioni Array       ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════╝"
    PRINT

    // Creiamo un array di ordini (di tipo Vendita) inizialmente vuoto.
    // L'array è dinamico e verrà ridimensionato dalla funzione helper.
    DIM Ordini(0) AS Vendita
    CreaDatiDiTest(Ordini)
    
    PRINT "--- 1. Set di Dati Iniziale ---"
    // AEVAL itera sull'array 'Ordini' e chiama la SUB lambda per ogni elemento 'o'.
    // È equivalente a un ciclo FOR EACH ma più conciso.
    AEVAL Ordini, SUB(o AS Vendita) { StampaVendita(o) }
    PRINT
    
    PRINT "--- 2. Filtriamo solo gli ordini attivi (non annullati) ---"
    // AFILTER crea un nuovo array 'OrdiniAttivi' contenente solo gli elementi per cui
    // la lambda restituisce TRUE. Qui usiamo la proprietà 'Annullato'.
    DIM OrdiniAttivi() AS Vendita = AFILTER(Ordini, FN(o AS Vendita) => NOT o.Annullato)
    PRINT "Trovati "; UBOUND(OrdiniAttivi) + 1; " ordini attivi."
    PRINT

    PRINT "--- 3. Calcoliamo il valore totale per ogni ordine attivo (Quantita * Prezzo) ---"
    // AMAP trasforma ogni elemento 'Vendita' in un valore 'CURRENCY' calcolato.
    // Il risultato è un nuovo array 'ValoriOrdini' che contiene solo gli importi.
    DIM ValoriOrdini() AS CURRENCY = AMAP(OrdiniAttivi, FN(o AS Vendita) => o.Quantita * o.PrezzoUnitario)
    
    PRINT "--- 4. Calcoliamo il fatturato totale sommando i valori di tutti gli ordini ---"
    // AREDUCE riduce l'array di valori 'ValoriOrdini' a un singolo scalare 'FatturatoTotale'.
    // La lambda somma l'accumulatore corrente con il valore dell'elemento.
    DIM FatturatoTotale AS CURRENCY = AREDUCE(ValoriOrdini, FN(totale AS CURRENCY, valore AS CURRENCY) => totale + valore)
    PRINT "Fatturato Totale degli ordini attivi: "; FORMAT(FatturatoTotale, "$#,##0.00")
    PRINT

    PRINT "--- 5. Troviamo il primo ordine di 'SSD NVMe' con una quantità superiore a 5 ---"
    // ASCAN cerca il primo elemento che soddisfa la condizione complessa definita nella lambda.
    // Restituisce l'indice dell'elemento o -1 se non trovato.
    DIM IndiceOrdineImportante = ASCAN(OrdiniAttivi, FN(o AS Vendita) => o.Prodotto = "SSD NVMe" AND o.Quantita > 5)
    IF IndiceOrdineImportante > -1 THEN
        PRINT "Trovato ordine importante all'indice "; IndiceOrdineImportante; ":"
        StampaVendita(OrdiniAttivi(IndiceOrdineImportante))
    ELSE
        PRINT "Nessun ordine di SSD NVMe con grandi quantità trovato."
    END IF
    PRINT

    PRINT "--- 6. Ordiniamo gli ordini attivi per valore totale (dal più alto al più basso) ---"
    // ASORT ordina l'array in-place. Poiché stiamo ordinando UDT complessi, dobbiamo fornire
    // una funzione di confronto (comparatore) che definisca l'ordine.
    // Qui ordiniamo in base al valore calcolato (Quantita * Prezzo) in ordine decrescente (b - a).
    LET Comparatore = FN(a AS Vendita, b AS Vendita) => SGN((b.Quantita * b.PrezzoUnitario) - (a.Quantita * a.PrezzoUnitario))
    ASORT OrdiniAttivi, Comparatore
    PRINT "Ordini attivi ordinati per valore decrescente:"
    AEVAL OrdiniAttivi, SUB(o AS Vendita) { StampaVendita(o) }

END SUB

// Helper per popolare l'array di test con dati fittizi.
// Usa REDIM per dimensionare l'array e chiamate a una factory function.
SUB CreaDatiDiTest(BYREF Dati() AS Vendita)
    REDIM Dati(5)
    Dati(0) = NuovoOrdine(101, "Mouse Gaming", 10, 25.50, FALSE)
    Dati(1) = NuovoOrdine(102, "Tastiera Meccanica", 5, 89.90, FALSE)
    Dati(2) = NuovoOrdine(103, "SSD NVMe", 2, 120.00, TRUE)  // Annullato
    Dati(3) = NuovoOrdine(104, "Monitor 27 pollici", 3, 250.00, FALSE)
    Dati(4) = NuovoOrdine(105, "SSD NVMe", 8, 115.50, FALSE) // Importante
    Dati(5) = NuovoOrdine(106, "Webcam HD", 20, 45.00, FALSE)
END SUB

// Factory Function: Helper per creare e inizializzare una nuova istanza di Vendita.
// Dimostra come le funzioni possono restituire tipi UDT.
FUNCTION NuovoOrdine(id AS LONG, prod$ AS STRING, qty AS INTEGER, price AS CURRENCY, canc AS BOOLEAN) AS Vendita
    DIM v AS Vendita
    v.ID = id
    v.Prodotto = prod$
    v.Quantita = qty
    v.PrezzoUnitario = price
    v.Annullato = canc
    RETURN v
END FUNCTION

// Helper per stampare un singolo ordine in modo formattato.
// Usa TAB per l'allineamento e FORMAT per la valuta.
SUB StampaVendita(o AS Vendita)
    LOCAL Valore AS CURRENCY = o.Quantita * o.PrezzoUnitario
    PRINT "  ID: "; o.ID; TAB(12);
    PRINT "Prod: "; o.Prodotto; TAB(45);
    PRINT "Qty: "; o.Quantita; TAB(55);
    PRINT "Valore: "; FORMAT(Valore, "#,##0.00");
    IF o.Annullato THEN PRINT " (ANNULLATO)" ELSE PRINT
END SUB