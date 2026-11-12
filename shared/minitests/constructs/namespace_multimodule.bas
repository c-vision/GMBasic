' EXPECT: A x= 2|B x= 2|A x dopo= 2
' Accesso qualificato `<Namespace>.<variabile>` fra moduli (spec Variables-Scope §1/§6,
' bug Namespace-Accesso-Qualificato-Fra-Moduli). Il modulo `namespace_multimodule_lib.inc`
' dichiara `NAMESPACE FooTest` con `PUBLIC DIM x` e lo inizializza a livello di modulo:
' - l'inizializzazione deve girare prima dell'entry point (init dei moduli importati);
' - `FooTest.x` deve leggere QUELLA variabile (2), non un simbolo inesistente (0 / "").
IMPORT FooTest

SUB Main()
    PRINT "A x="; FooTest.x
    ShowB()
    PRINT "A x dopo="; FooTest.x
END SUB
