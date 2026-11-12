' Modulo del MiniTest constructs/namespace_multimodule.bas (estensione .inc: NON è un test).
' Bug Namespace-Accesso-Qualificato-Fra-Moduli: il codice di modulo di un file importato
' deve essere eseguito PRIMA dell'entry point, e `<Ns>.<variabile>` deve risolvere alla
' variabile PUBLIC del namespace (non a una variabile inesistente).
NAMESPACE FooTest
PUBLIC DIM x AS LONG
x = 2
PUBLIC SUB ShowB()
    PRINT "B x="; x
END SUB
