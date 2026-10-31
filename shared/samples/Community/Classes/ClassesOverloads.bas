' FILE: Classes/ClassesOverloads.bas
' =================================================================================================
'       GUIDA TEORICA: OVERLOADING DEGLI OPERATORI (TODO #19, #20, #21)
' =================================================================================================
'
' Questo programma testa in modo esaustivo la funzionalità di overloading degli operatori
' per le classi, coprendo operatori aritmetici, unari, di confronto e bitwise.
'
' 1. COS'È L'OVERLOADING DEGLI OPERATORI?
'    È la capacità di definire un comportamento personalizzato per gli operatori standard
'    (come +, -, =, AND) quando vengono applicati a istanze di una classe.
'    Questo permette di scrivere codice più intuitivo e leggibile.
'    Esempio: `dim p3 as Punto = p1 + p2` è più chiaro di `dim p3 as Punto = Punto.Add(p1, p2)`.
'
' 2. SINTASSI
'    `PUBLIC [STATIC] OPERATOR <simbolo> (parametri) AS <TipoRitorno> ... END OPERATOR`
'    - Operatori Binari (es. +): Richiedono due parametri.
'    - Operatori Unari (es. -): Richiedono un solo parametro.
'    - Devono essere PUBLIC e, nella maggior parte dei casi, STATIC.
'
' 3. TODO VERIFICATI IN QUESTO TEST:
'
'    - TODO #19: OPERATORI UNARI (Negazione)
'      - Viene definito `OPERATOR -(a AS Punto)` per invertire le coordinate di un punto.
'      - Test: `DIM pNeg as Punto = -p1`
'
'    - TODO #20: OPERATORI DI CONFRONTO
'      - Vengono definiti `OPERATOR =`, `OPERATOR <>`, `OPERATOR >`, `OPERATOR <`.
'      - Il confronto si basa sulla distanza del punto dall'origine.
'      - Test: `IF p1 = p2 THEN ...`, `IF p1 > p3 THEN ...`
'
'    - TODO #21: OPERATORI BITWISE
'      - Vengono definiti `OPERATOR AND`, `OPERATOR OR` (binari) e `OPERATOR NOT` (unario).
'      - Vengono usati in modo "creativo" per testare il meccanismo:
'        - AND: Restituisce un punto con le coordinate minime tra i due operandi.
'        - OR: Restituisce un punto con le coordinate massime.
'        - NOT: Restituisce un punto con le coordinate scambiate (X,Y -> Y,X).
'      - Test: `dim pAnd = p1 AND p2`, `dim pNot = NOT p1`
'
' =================================================================================================

IMPORT SYSTEM ' Per ABS()

CLASS Punto
    PUBLIC DIM X AS DOUBLE
    PUBLIC DIM Y AS DOUBLE
    
    PUBLIC SUB NEW(x_val AS DOUBLE, y_val AS DOUBLE)
        SELF.X = x_val
        SELF.Y = y_val
    END SUB
    
    ' Metodo helper per stampare le coordinate
    PUBLIC SUB Print(messaggio AS STRING)
        PRINT messaggio; ": ("; SELF.X; ", "; SELF.Y; ")"
    END SUB
    
    ' --- OPERATORI ARITMETICI (Già implementati) ---
    PUBLIC STATIC OPERATOR +(a AS Punto, b AS Punto) AS Punto
        OPERATOR = NEW Punto(a.X + b.X, a.Y + b.Y)
    END OPERATOR
    
    ' --- TODO #19: OPERATORE UNARIO ---
    PUBLIC STATIC OPERATOR -(a AS Punto) AS Punto
        OPERATOR = NEW Punto(-a.X, -a.Y)
    END OPERATOR
    
    ' --- TODO #20: OPERATORI DI CONFRONTO ---
    ' Il confronto si basa sulla distanza dall'origine (magnitudine)
    PUBLIC STATIC OPERATOR =(a AS Punto, b AS Punto) AS BIT
        OPERATOR = (a.X = b.X AND a.Y = b.Y)
    END OPERATOR
    
    PUBLIC STATIC OPERATOR <>(a AS Punto, b AS Punto) AS BIT
        OPERATOR = NOT (a = b)
    END OPERATOR
    
    PUBLIC STATIC OPERATOR >(a AS Punto, b AS Punto) AS BIT
        DIM magA = SQR(a.X^2 + a.Y^2)
        DIM magB = SQR(b.X^2 + b.Y^2)
        OPERATOR = magA > magB
    END OPERATOR

    PUBLIC STATIC OPERATOR <(a AS Punto, b AS Punto) AS BIT
        DIM magA = SQR(a.X^2 + a.Y^2)
        DIM magB = SQR(b.X^2 + b.Y^2)
        OPERATOR = magA < magB
    END OPERATOR

    ' --- TODO #21: OPERATORI BITWISE ---
    ' Usati creativamente per testare il meccanismo
    PUBLIC STATIC OPERATOR AND(a AS Punto, b AS Punto) AS Punto
        ' "AND logico" sulle coordinate: prende il minimo
        OPERATOR = NEW Punto(MIN(a.X, b.X), MIN(a.Y, b.Y))
    END OPERATOR
    
    PUBLIC STATIC OPERATOR OR(a AS Punto, b AS Punto) AS Punto
        ' "OR logico" sulle coordinate: prende il massimo
        OPERATOR = NEW Punto(MAX(a.X, b.X), MAX(a.Y, b.Y))
    END OPERATOR
    
    PUBLIC STATIC OPERATOR NOT(a AS Punto) AS Punto
        ' "NOT logico": scambia le coordinate
        OPERATOR = NEW Punto(a.Y, a.X)
    END OPERATOR
    
END CLASS

SUB Main()
    DIM p1 AS Punto = NEW Punto(10, 20)
    DIM p2 AS Punto = NEW Punto(3, 4)
    DIM p3 AS Punto = NEW Punto(10, 20)
    
    p1.Print("Punto p1")
    p2.Print("Punto p2")
    p3.Print("Punto p3")
    PRINT
    
    PRINT "--- Test Operatori Aritmetici (già funzionanti) ---"
    DIM pSum AS Punto = p1 + p2
    pSum.Print("p1 + p2")
    PRINT

    PRINT "--- Test TODO #19: Operatore Unario (-) ---"
    DIM pNeg AS Punto = -p1
    pNeg.Print("-p1")
    PRINT

    PRINT "--- Test TODO #20: Operatori di Confronto (=, <>, >) ---"
    IF p1 = p3 THEN PRINT "Verifica 'p1 = p3' : OK (Corretto)" ELSE PRINT "Verifica 'p1 = p3' : ERRORE"
    IF p1 <> p2 THEN PRINT "Verifica 'p1 <> p2': OK (Corretto)" ELSE PRINT "Verifica 'p1 <> p2': ERRORE"
    IF p1 > p2 THEN PRINT "Verifica 'p1 > p2' : OK (Corretto, p1 è più lontano dall'origine)" ELSE PRINT "Verifica 'p1 > p2' : ERRORE"
    PRINT
    
    PRINT "--- Test TODO #21: Operatori Bitwise (AND, OR, NOT) ---"
    DIM pAnd AS Punto = p1 AND p2
    pAnd.Print("p1 AND p2 (dovrebbe essere min(X), min(Y))")
    
    DIM pOr AS Punto = p1 OR p2
    pOr.Print("p1 OR p2  (dovrebbe essere max(X), max(Y))")
    
    DIM pNot AS Punto = NOT p1
    pNot.Print("NOT p1    (dovrebbe scambiare X e Y)")
    PRINT
    
    PRINT "Test degli overload completato."
    REM WAITKEY
END SUB
