' FILE: Functions/FunctionsLambdaInference.bas
' ======================================================================================================================
' TEST UNITARIO: INFERENZA DEI TIPI NELLE LAMBDA EXPRESSION
' ======================================================================================================================
' Questo file verifica che il compilatore (specificamente il Linker/Rewriter) sia in grado
' di inferire correttamente i tipi di dato dei parametri delle lambda (e dei code block)
' in base al contesto in cui vengono utilizzate.
'
' OBIETTIVO:
' L'AST finale generato da questo file NON DEVE contenere tipi '<inferred>'
' nei parametri delle lambda.
' ======================================================================================================================

IMPORT ARRAYS
IMPORT SYSTEM

' Definisce un tipo di dato per i test
TYPE Cliente
    ID AS LONG
    Nome AS STRING * 30
    Saldo AS CURRENCY
END TYPE

' Definisce un tipo delegato per testare l'assegnazione
DELEGATE FUNCTION FiltroCliente(c AS Cliente) AS BIT

SUB Main()
    CLS
    PRINT "--- Test Inferenza Tipi Lambda ---"
    PRINT

    ' --- TEST 1: AEVAL con array di tipi primitivi ---
    PRINT "[1] Test con AEVAL su Array di DOUBLE"
    DIM Numeri = {10.5, 20.2, 30.8}
    ' Il compilatore deve inferire che 'n' è di tipo DOUBLE
    AEVAL Numeri, { |n| PRINT "  -> Valore double: "; n }
    PRINT

    ' --- TEST 2: AEVAL con array di UDT ---
    PRINT "[2] Test con AEVAL su Array di UDT (Cliente)"
    DIM Clienti(0) AS Cliente
    Clienti(0).Nome = "Test Cliente"
    Clienti(0).Saldo = 123.45
    ' Il compilatore deve inferire che 'rec' è di tipo Cliente
    AEVAL Clienti, { |rec| PRINT "  -> Nome cliente: "; rec.Nome; ", Saldo: "; rec.Saldo }
    PRINT

    ' --- TEST 3: AFILTER (restituisce un nuovo array) ---
    PRINT "[3] Test con AFILTER su Array di INTEGER"
    DIM Voti = {4, 8, 5, 9, 7}
    ' Il compilatore deve inferire che 'voto' è di tipo DOUBLE (default per literal) o INTEGER
    DIM VotiAlti = AFILTER(Voti, { |voto| RETURN voto >= 6 })
    PRINT "  -> Voti sufficienti: ";
    AEVAL VotiAlti, { |v| PRINT v; " "; }
    PRINT : PRINT
    
    ' --- TEST 4: ASCAN (cerca un elemento) ---
    PRINT "[4] Test con ASCAN su Array di STRING"
    DIM Nomi = {"Mario", "Luigi", "Anna"}
    ' Il compilatore deve inferire che 'nome' è di tipo STRING
    DIM pos = ASCAN(Nomi, { |nome| RETURN nome = "Anna" })
    PRINT "  -> Posizione di 'Anna': "; pos; " (Atteso: 2)"
    PRINT

    ' --- TEST 5: Assegnazione a Delegato (Target Typing esplicito) ---
    PRINT "[5] Test con assegnazione a Delegato"
    DIM mioFiltro AS FiltroCliente
    ' Il compilatore deve usare la firma di 'FiltroCliente' per inferire che 'c' è di tipo Cliente.
    mioFiltro = { |c| RETURN c.Saldo > 0 }
    
    ' Simulazione di utilizzo del delegato
    DIM clienteTest AS Cliente
    clienteTest.Saldo = 100
    ' La chiamata a EVAL qui è solo per dimostrazione, il test è la compilazione.
    ' IF EVAL(mioFiltro, {clienteTest}) THEN PRINT "  -> Delegato eseguito con successo."
    PRINT "  -> Assegnazione a delegato compilata con successo."
    PRINT

    PRINT "--- Test completato ---"
END SUB