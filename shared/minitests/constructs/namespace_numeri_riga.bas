' EXPECT: COMPILE ERROR: Line numbers not allowed inside NAMESPACE 'NumLineTest'.
' MiniTest NEGATIVO di compilazione: un MODULO (file con `NAMESPACE`) non può avere numeri di
' riga. Il codice numerato legacy appartiene al programma principale (righe < 1_000_000), non a
' una libreria importata: il parser lo rifiuta dentro `NAMESPACE`, con lo stesso testo su C# e C
' (parità 4/4). Un modulo importato è quindi sempre privo di numeri di riga e la sua
' inizializzazione (codice di modulo) è fatta solo di righe senza numero.
NAMESPACE NumLineTest
10 PRINT "non deve compilare"

SUB Main()
    PRINT "irraggiungibile"
END SUB
