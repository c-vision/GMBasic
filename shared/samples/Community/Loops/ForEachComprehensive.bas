' FILE: Loops/ForEachComprehensive.bas
' =================================================================================================
'       FOR EACH — TUTTE LE FORME SUPPORTATE
' =================================================================================================
' Questo file dimostra TUTTE le varianti di FOR EACH disponibili in GM-Basic/GMBasicAI.
' Ogni sezione è indipendente e testa una forma specifica.
' =================================================================================================

IMPORT JSON

TYPE Person
    Name AS STRING * 30
    Age AS INTEGER
END TYPE

SUB Main()

    ' ============================================================================
    ' 1. Array — tipi primitivi
    ' ============================================================================
    PRINT "--- 1a. Array di DOUBLE ---"
    DIM nums(4) AS DOUBLE = {1.1, 2.2, 3.3, 4.4, 5.5}
    FOR EACH n IN nums()
        PRINT n
    NEXT

    PRINT "--- 1b. Array di INTEGER ---"
    DIM ages(2) AS INTEGER = {25, 40, 60}
    FOR EACH a IN ages()
        PRINT a
    NEXT

    PRINT "--- 1c. Array di STRING ---"
    DIM names(2) AS STRING = {"Alice", "Bob", "Charlie"}
    FOR EACH name IN names()
        PRINT name
    NEXT

    ' ============================================================================
    ' 2. Array di UDT (User-Defined Type)
    ' ============================================================================
    PRINT "--- 2. Array di UDT ---"
    DIM people(1) AS Person
    people(0).Name = "Mario"
    people(0).Age = 42
    people(1).Name = "Luisa"
    people(1).Age = 35

    FOR EACH p IN people()
        PRINT p.Name, p.Age
    NEXT

    ' ============================================================================
    ' 3. LIST — generica e tipizzata
    ' ============================================================================
    PRINT "--- 3a. LIST tipizzata ---"
    DIM customers AS LIST OF Person
    DIM c1 AS Person : c1.Name = "Anna" : c1.Age = 28
    DIM c2 AS Person : c2.Name = "Marco" : c2.Age = 33
    customers.Add(c1)
    customers.Add(c2)

    FOR EACH cust IN customers
        PRINT cust.Name, cust.Age
    NEXT

    PRINT "--- 3b. LIST of STRING ---"
    DIM colors AS LIST OF STRING
    colors.Add("Red")
    colors.Add("Green")
    colors.Add("Blue")
    FOR EACH color IN colors
        PRINT UPPER(color)
    NEXT

    PRINT "--- 3c. LIST of DOUBLE ---"
    DIM prices AS LIST OF DOUBLE = {9.99, 19.99, 29.99}
    FOR EACH price IN prices
        PRINT price
    NEXT

    ' ============================================================================
    ' 4. DICT — chiave e valore
    ' ============================================================================
    PRINT "--- 4a. DICT of STRING, DOUBLE ---"
    DIM menu AS DICT OF STRING, DOUBLE = {"Pizza": 8.50, "Pasta": 7.00, "Vino": 5.00}
    FOR EACH item, cost IN menu
        PRINT item, cost
    NEXT

    PRINT "--- 4b. DICT of STRING, STRING ---"
    DIM capitals AS DICT OF STRING, STRING = {"Italy": "Rome", "France": "Paris", "Spain": "Madrid"}
    FOR EACH country, city IN capitals
        PRINT country, "->", city
    NEXT

    PRINT "--- 4c. Solo chiavi ---"
    FOR EACH KEY country IN capitals
        PRINT country
    NEXT

    PRINT "--- 4d. Solo valori ---"
    FOR EACH VALUE city IN capitals
        PRINT city
    NEXT

    ' ============================================================================
    ' 5. JSON
    ' ============================================================================
    PRINT "--- 5a. JSON Array ---"
    DIM jsonArr AS JSON = JSON.PARSE("[10, 20, 30, 40, 50]")
    FOR EACH val IN jsonArr
        PRINT val
    NEXT

    PRINT "--- 5b. JSON Object (valori) ---"
    DIM jsonObj AS JSON = JSON.PARSE("{ ""name"": ""John"", ""age"": 30, ""city"": ""Rome"" }")
    FOR EACH v IN jsonObj
        PRINT v
    NEXT

    PRINT "--- 5c. JSON Object (chiave + valore) ---"
    FOR EACH k, v IN jsonObj
        PRINT k, ":", v
    NEXT

    ' ============================================================================
    ' 6. STRING — iterazione per carattere
    ' ============================================================================
    PRINT "--- 6. Stringa ---"
    DIM text AS STRING = "BASIC"
    FOR EACH ch IN text
        PRINT ch, "=", ASC(ch)
    NEXT

    ' ============================================================================
    ' 7. FOR EACH con indice (array)
    ' ============================================================================
    PRINT "--- 7. Indice + valore su array ---"
    DIM fruit(2) AS STRING = {"Apple", "Banana", "Cherry"}
    FOR EACH idx, f IN fruit()
        PRINT idx, f
    NEXT

    ' ============================================================================
    ' 8. REVERSE — iterazione invertita
    ' ============================================================================
    PRINT "--- 8. REVERSE ---"
    FOR EACH item IN REVERSE(colors)
        PRINT item
    NEXT

    ' ============================================================================
    ' 9. RANGE — sequenza numerica
    ' ============================================================================
    ' NOTA: l'accesso RDD/SQL reale usa "SQL OPEN ... ALIAS" + "DO WHILE SQL.FETCH(rs)"
    ' (vedi Databases/SqlMinimalCrud.bas), non FOR EACH — un RECORDSET non è una forma
    ' FOR EACH supportata, quindi non è coperta da questo sample.
    PRINT "--- 9. RANGE(1, 5) ---"
    FOR EACH n IN RANGE(1, 5)
        PRINT n
    NEXT

    ' ============================================================================
    ' 10. Collezioni annidate
    ' ============================================================================
    PRINT "--- 10. Nested FOR EACH ---"
    DIM matrix(1, 2) AS DOUBLE = { {1,2,3}, {4,5,6} }
    FOR EACH row IN RANGE(0, 1)
        FOR EACH col IN RANGE(0, 2)
            PRINT matrix(row, col),
        NEXT
        PRINT
    NEXT

    PRINT
    PRINT "FOR EACH comprehensive test completed."

END SUB
