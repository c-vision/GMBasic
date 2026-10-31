' FILE: Arrays/ArraysAsortComparer.bas
' =================================================================================================
' ASORT con COMPARATORE su array di UDT (TYPE) e su array numerici.
' =================================================================================================
' `ASORT arr, { |a, b| RETURN predicato }` ordina arr IN-PLACE.
' Il comparatore BASIC è un PREDICATO booleano: TRUE (= -1) significa "a va PRIMA di b".
'   { |a, b| RETURN a.Campo > b.Campo }  -> ordine DECRESCENTE per Campo
'   { |a, b| RETURN a.Campo < b.Campo }  -> ordine CRESCENTE per Campo
' È anche accettato un comparatore a 3 vie (negativo = a prima, 0 = uguali, positivo = a dopo),
'   es. { |a, b| RETURN a.Campo - b.Campo }.
' L'ordine esplicito è ottenibile con la clausola opzionale: ASORT arr, cmp, "D" (discendente).
' =================================================================================================

TYPE Prodotto
    Nome AS STRING * 20
    Prezzo AS CURRENCY
    Quantita AS LONG
END TYPE

SUB Main()
    DIM Prodotti(2) AS Prodotto
    Prodotti(0).Nome = "Penna"    : Prodotti(0).Prezzo = 1.5 : Prodotti(0).Quantita = 100
    Prodotti(1).Nome = "Monitor"  : Prodotti(1).Prezzo = 250 : Prodotti(1).Quantita = 3
    Prodotti(2).Nome = "Tastiera" : Prodotti(2).Prezzo = 90  : Prodotti(2).Quantita = 10

    PRINT "--- Ordine iniziale ---"
    AEVAL Prodotti, { |p| PRINT "  "; p.Nome; " : "; p.Prezzo }

    ' 1. Predicato decrescente per prezzo (TRUE = a prima di b).
    ASORT Prodotti, { |a, b| RETURN a.Prezzo > b.Prezzo }
    PRINT "--- Per prezzo (decrescente) ---"
    AEVAL Prodotti, { |p| PRINT "  "; p.Nome; " : "; p.Prezzo }

    ' 2. Predicato crescente per quantità (TRUE = a prima di b).
    ASORT Prodotti, { |a, b| RETURN a.Quantita < b.Quantita }
    PRINT "--- Per quantita' (crescente) ---"
    AEVAL Prodotti, { |p| PRINT "  "; p.Nome; " : "; p.Quantita }

    ' 3. ASORT su array numerico con comparatore a 3 vie (decrescente).
    DIM Numeri = {3, 1, 4, 1, 5, 9, 2, 6}
    ASORT Numeri, { |a, b| RETURN b - a }
    PRINT "--- Numeri (3-vie, decrescente) ---"
    AEVAL Numeri, { |n| PRINT "  "; n }

    ' 4. ASORT su array numerico senza comparatore (ascendente di default).
    ASORT Numeri
    PRINT "--- Numeri (default, crescente) ---"
    AEVAL Numeri, { |n| PRINT "  "; n }
END SUB
