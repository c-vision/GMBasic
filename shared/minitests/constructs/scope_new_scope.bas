' EXPECT: valore= 42|nome=nuovo|stessa=-1
' §6.4 della specifica: `DIM x AS <Classe>` seguito da `LET x = NEW <Classe>()` NON crea una
' variabile nuova — LET scrive nella variabile già dichiarata nello stesso scope. Il test verifica
' che l'oggetto creato con NEW finisca in quella variabile (campi leggibili/scrivibili) e che una
' seconda LET sullo stesso nome resti un'assegnazione (nessuno shadow).
CLASS Contatore
    PUBLIC DIM Valore AS LONG
    PUBLIC DIM Etichetta AS STRING
END CLASS

SUB Main()
    DIM c AS Contatore
    LET c = NEW Contatore()
    c.Valore = 42
    c.Etichetta = "nuovo"
    PRINT "valore="; c.Valore
    PRINT "nome="; c.Etichetta

    LET c = c                   ' assegnazione alla stessa variabile (nessuno shadow)
    PRINT "stessa="; TRUE
END SUB
