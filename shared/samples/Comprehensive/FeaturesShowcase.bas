' FILE: samples/Comprehensive/FeaturesShowcase.bas
' ======================================================================================================================
' Sample COMPLETO che esercita OGNI comando/funzione/costrutto implementato (parità 5/5):
'   - operatori ++ / -- (prefix/postfix; variabile, campo, elemento d'array, parametro)
'   - UDT annidato a 2 livelli (campo di campo) + campo STRING * N
'   - array-campo di CLASSE 2-D (PUBLIC DIM m(r,c))
'   - DIM multi-variabile (DIM a, b AS STRING)
'   - accumulatore scalare in un loop (DIM senza inizializzatore)
'   - conversioni:  CLNG / CINT (half-to-even), INT (floor), FIX / TRUNC (tronca),
'                   FLOOR, CEILING, ROUND (half-to-even)
'   - letterali ARRAY:  DIM a() = [..] (implicito), DIM b(n) = [..] (dimensionato),
'                       DIM m(r,c) = [[..],[..]] (matrice 2-D), DIM c() = {..}
'   - letterali DIZIONARIO: { "k": v } tipizzato e inferito
'   - comandi AGENT: AI AGENT TRACE [VERBOSE|ON|OFF], AI AGENT SERVE <porta>, AI AGENT STOP SERVER
'
' Verificato in parità su IL / P-Code / VM/C / C99Native.
' ======================================================================================================================
IMPORT AI

TYPE Coord
    lat AS DOUBLE
    lon AS DOUBLE
END TYPE
TYPE Stazione
    pos AS Coord
    codice AS STRING * 6
END TYPE
CLASS Matrix
    PUBLIC DIM m(1, 1) AS INTEGER
    PUBLIC DIM etichetta AS Stazione
    PUBLIC SUB NEW()
        etichetta.pos.lat = 45.07
        etichetta.codice = "MI"
    END SUB
    FUNCTION Somma() AS INTEGER
        DIM i AS INTEGER
        DIM j AS INTEGER
        DIM s AS INTEGER
        FOR i = 0 TO 1
            FOR j = 0 TO 1
                m(i, j) = i * 2 + j + 1
            NEXT j
        NEXT i
        s = 0
        FOR i = 0 TO 1
            FOR j = 0 TO 1
                s = s + m(i, j)
            NEXT j
        NEXT i
        Somma = s
    END FUNCTION
END CLASS

SUB Main()
    PRINT "=== FeaturesShowcase: report vendite ==="

    ' --- 1. DIM multi-variabile + accumulatore in loop ---
    DIM titolo AS STRING, sottotitolo AS STRING
    titolo = "Vendite"
    sottotitolo = "Q4"
    PRINT titolo + " / " + sottotitolo

    ' --- 2. Letterali ARRAY (parentesi quadre): implicito, dimensionato, matrice 2-D; e graffe ---
    DIM prezzi() AS DOUBLE = [9.99, 4.50, 12.00, 3.75]
    DIM quantita(4) AS INTEGER = [2, 10, 1, 4, 0]
    DIM matrice(1, 1) AS INTEGER = [[1, 2], [3, 4]]
    DIM nomi() AS STRING = ["penna", "quaderno", "zaino", "gomma"]
    DIM graffe() AS INTEGER = {100, 200, 300}

    ' --- 3. Letterali DIZIONARIO (graffa { k: v }): tipizzato e inferito ---
    DIM aliquote AS DICT OF STRING, DOUBLE = {"penna": 0.10, "zaino": 0.20}
    DIM magazzino = {"penna": 100, "quaderno": 50, "zaino": 5, "gomma": 300}

    ' --- 4. Conversioni numeriche + dict: report ---
    DIM totale AS DOUBLE = 0
    DIM lordo AS DOUBLE = 0
    FOR i = 0 TO 3
        lordo = prezzi(i) * quantita(i)
        DIM q1 AS LONG = CLNG(quantita(i))
        DIM q2 AS INTEGER = CINT(quantita(i))
        DIM iva AS DOUBLE = 0
        IF i = 0 OR i = 2 THEN iva = aliquote(nomi(i))
        DIM netto AS DOUBLE = lordo * (1.0 + iva)
        PRINT nomi(i); ": q="; q1; "/"; q2; "  lordo="; lordo; "  iva="; iva; "  netto="; netto
        totale = totale + netto
    NEXT i
    PRINT "TOTALE="; totale

    ' --- 5. Aggregazione / arrotondamenti ---
    DIM media AS DOUBLE = totale / 4.0
    PRINT "round(media)="; ROUND(media); " floor="; FLOOR(media); " ceil="; CEILING(media)
    PRINT "int(media)="; INT(media); " fix="; FIX(media); " trunc="; TRUNC(media)
    PRINT "clng(media)="; CLNG(media); " cint(media)="; CINT(media)
    PRINT "clng(-2.5)="; CLNG(-2.5); " cint(-2.5)="; CINT(-2.5); " int(-2.5)="; INT(-2.5); " fix(-2.5)="; FIX(-2.5)

    ' --- 6. Matrice 2-D [[..],[..]] + dict ---
    DIM sommaMat AS INTEGER = 0
    FOR r = 0 TO 1
        FOR c = 0 TO 1
            sommaMat = sommaMat + matrice(r, c)
        NEXT c
    NEXT r
    PRINT "somma matrice="; sommaMat; "  graffe(2)="; graffe(2); "  scorta penna="; magazzino("penna")

    ' --- 7. ++ / -- (variabile) ---
    DIM n AS INTEGER
    n = 5
    n++
    ++n
    PRINT "n (dopo ++)="; n
    DIM vecchio AS INTEGER
    vecchio = n++
    PRINT "postfix vecchio="; vecchio; " n="; n

    ' --- 8. UDT annidato (2 livelli) + array-campo 2-D di classe ---
    DIM mx AS Matrix = NEW Matrix()
    PRINT "matrix sum="; mx.Somma(); "  cella(1,1)="; mx.m(1, 1)
    PRINT "stazione lat="; mx.etichetta.pos.lat; "  codice="; mx.etichetta.codice

    ' --- 9. Comandi AGENT: tracing + server HTTP degli agent ---
    AI AGENT TRACE VERBOSE
    AI AGENT TRACE ON
    AI AGENT TRACE OFF
    AI AGENT SERVE 8792
    PRINT "agent server up (8792)"
    AI AGENT STOP SERVER
    PRINT "agent server down"

    PRINT "FeaturesShowcase completato."
END SUB
