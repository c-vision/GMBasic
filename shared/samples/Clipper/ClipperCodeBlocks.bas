' FILE: Clipper/ClipperCodeBlocks.bas
' ======================================================================================================================
' TEST COMPLETO E SISTEMATICO DEI CODE BLOCK (LAMBDA EXPRESSIONS)
' ======================================================================================================================
' Questo file verifica l'implementazione della sintassi dei code block `{ |params| corpo }`.
' Copre tutti gli scenari di inferenza e utilizzo supportati dal compilatore:
' 1. Iterazione (AEVAL) con tipi primitivi e UDT.
' 2. Trasformazione e Filtraggio (AMAP, AFILTER).
' 3. Aggregazione e Ricerca (AREDUCE, ACOUNT, ASCAN).
' 4. Comparazione Custom (ASORT).
' 5. Callback per funzioni Matematiche e di Ottimizzazione.
' 6. Tipi Espliciti, Assegnazione a Variabili e Chiamata Indiretta.
' ======================================================================================================================

IMPORT ARRAYS
IMPORT COLLECTIONS
IMPORT OPTIMIZE

SUB Main()

    ' --- Setup Iniziale ---
    TYPE Cliente
        ID AS LONG
        Nome AS STRING * 30
        Fatturato AS CURRENCY
    END TYPE

    PRINT "╔══════════════════════════════════════════════════════╗"
    PRINT "║   TEST COMPLETO CODE BLOCKS (Lambda Expressions)     ║"
    PRINT "╚══════════════════════════════════════════════════════╝"
    PRINT

    ' ======================================================
    ' 1. TEST ITERAZIONE (AEVAL)
    ' ======================================================
    PRINT "--- 1. Test Iterazione (AEVAL) ---"
    DIM Numeri = {10, 20, 30}
    
    PRINT "1.1: AEVAL con 1 parametro (valore)"
    AEVAL Numeri, { |n| PRINT "  -> Valore: "; n }
    PRINT
    
    PRINT "1.2: AEVAL con 2 parametri (valore, indice)"
    AEVAL Numeri, { |valore, indice| PRINT "  -> Indice: "; indice; ", Valore: "; valore }
    PRINT

    PRINT "1.3: AEVAL su UDT"
    DIM Clienti(1) AS Cliente
    Clienti(0).Nome = "Mario" : Clienti(0).Fatturato = 1000
    Clienti(1).Nome = "Luigi" : Clienti(1).Fatturato = 1500
    ' Compilatore deve inferire 'c' come TYPE Cliente
    AEVAL Clienti, { |c| PRINT "  -> Cliente: "; c.Nome; ", Fatturato: "; c.Fatturato }
    PRINT

    ' ======================================================
    ' 2. TEST TRASFORMAZIONE E FILTRAGGIO (AMAP, AFILTER)
    ' ======================================================
    PRINT "--- 2. Test Trasformazione (AMAP) e Filtro (AFILTER) ---"
    DIM Sorgente = {1, 2, 3, 4, 5, 6}
    PRINT "Array sorgente: {1, 2, 3, 4, 5, 6}"

    ' AMAP: il tipo di 'x' è inferito da 'Sorgente' (DOUBLE)
    DIM Quadrati = AMAP(Sorgente, { |x| RETURN x * x })
    PRINT "Quadrati (via AMAP):"
    AEVAL Quadrati, { |q| PRINT "  -> "; q }
    PRINT

    ' AFILTER: il tipo di 'x' è inferito da 'Quadrati' (DOUBLE)
    DIM Pari = AFILTER(Quadrati, { |x| RETURN (x MOD 2) = 0 })
    PRINT "Numeri Pari (via AFILTER):"
    AEVAL Pari, { |p| PRINT "  -> "; p }
    PRINT

    ' ======================================================
    ' 3. TEST AGGREGAZIONE E RICERCA (AREDUCE, ACOUNT, ASCAN)
    ' ======================================================
    PRINT "--- 3. Test Aggregazione e Ricerca ---"
    DIM Valori = {1, 5, 2, 8, 3}
    PRINT "Array: {1, 5, 2, 8, 3}"

    ' AREDUCE: 'acc' e 'curr' inferiti da 'Valori' e 'initialValue'
    DIM Somma = AREDUCE(Valori, { |acc, curr| RETURN acc + curr }, 0)
    PRINT "Somma (via AREDUCE): "; Somma; " (Atteso: 19)"
    
    ' ACOUNT: 'v' inferito da 'Valori'
    DIM MaggioriDiTre = ACOUNT(Valori, { |v| RETURN v > 3 })
    PRINT "Elementi > 3 (via ACOUNT): "; MaggioriDiTre; " (Atteso: 2)"
    
    ' ASCAN: 'v' inferito da 'Valori'
    DIM PosizioneDiOtto = ASCAN(Valori, { |v| RETURN v = 8 })
    PRINT "Posizione del valore 8 (via ASCAN): "; PosizioneDiOtto; " (Atteso: 4)"
    PRINT

    ' ======================================================
    ' 4. TEST ORDINAMENTO CUSTOM (ASORT)
    ' ======================================================
    PRINT "--- 4. Test ASORT con Comparatore Custom ---"
    DIM ClientiOrdinamento(1) AS Cliente
    ClientiOrdinamento(0).Nome = "Zorro" : ClientiOrdinamento(0).Fatturato = 500
    ClientiOrdinamento(1).Nome = "Abaco" : ClientiOrdinamento(1).Fatturato = 2000
    
    PRINT "Clienti non ordinati:"
    AEVAL ClientiOrdinamento, { |c| PRINT "  -> "; c.Nome; " (", c.Fatturato, ")" }
    
    ' Ordina per Fatturato decrescente. 'a' e 'b' sono inferiti come TYPE Cliente.
    ASORT ClientiOrdinamento, { |a, b| RETURN a.Fatturato > b.Fatturato }
    
    PRINT "Ordinati per Fatturato (Decrescente):"
    AEVAL ClientiOrdinamento, { |c| PRINT "  -> "; c.Nome; " (", c.Fatturato, ")" }
    PRINT
    
    ' ======================================================
    ' 5. TEST CALLBACK MATEMATICHE (ANALYSIS, OPTIMIZE)
    ' ======================================================
    PRINT "--- 5. Test Callback Fortemente Tipizzate ---"
    ' NUMERICAL.Integrate e OPTIMIZE.* si aspettano f(x AS DOUBLE) AS DOUBLE.
    ' Il compilatore deve iniettare il tipo DOUBLE nel parametro 'x' della lambda.
    
    DIM Area = NUMERICAL.Integrate({ |x| RETURN x * x }, 0.0, 1.0)
    PRINT "Integrale di x^2 da 0 a 1: "; Area; " (Atteso: ~0.333)"
    
    DIM Derivata = NUMERICAL.Differentiate({ |x| RETURN x ^ 3 }, 2.0)
    PRINT "Derivata di x^3 per x=2: "; Derivata; " (Atteso: 12)"
    
    ' Trova la radice di f(x) = x^2 - 4 (che è 2) partendo da un'ipotesi di 1.
    DIM Radice = OPTIMIZE.NEWTON({ |x| RETURN x^2 - 4 }, 1.0)
    PRINT "Radice di x^2 - 4 (via Newton): "; Radice; " (Atteso: ~2.0)"
    PRINT

    ' ======================================================
    ' 6. TEST TIPI ESPLICITI, ASSEGNAZIONE E CHIAMATA INDIRETTA
    ' ======================================================
    PRINT "--- 6. Test Tipi Espliciti e Chiamata Indiretta ---"
    ' Quando assegniamo una lambda a una variabile, non c'è un contesto
    ' da cui dedurre i tipi. Dobbiamo usare la sintassi AS TIPO.
    
    DIM CalcolaIpotenusa = { |a AS DOUBLE, b AS DOUBLE| RETURN SQR(a*a + b*b) }
    
    ' Invochiamo la variabile come se fosse una funzione.
    DIM Ipotenusa = CalcolaIpotenusa(3.0, 4.0)
    
    PRINT "Ipotenusa di (3, 4) calcolata tramite variabile: "; Ipotenusa; " (Atteso: 5)"
    PRINT

    PRINT "Test Completo dei Code Block terminato con successo."

END SUB