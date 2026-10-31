' FILE: Scopes/ScopesLetVsImplicit.bas
' Test: LET keyword vs implicit assignment behavior per specifica VARIABLES #3
' - LET: crea/shadows nello scope corrente (sempre nuovo simbolo locale)
' - Implicita: cerca verso l'alto, modifica se trovato, crea in scope corrente se non trovato

IMPORT SYSTEM

DIM GlobalA AS INTEGER = 100
DIM GlobalB AS INTEGER = 200
DIM GlobalC AS INTEGER = 300

SUB TestLetShadowing()
    ' LET su variabile con nome uguale a globale → crea locale (shadowing)
    LET GlobalA = 999
    PRINT "Dentro TestLetShadowing (LET): GlobalA ="; GlobalA
END SUB

SUB TestImplicitModifiesOuter()
    ' Assegnazione implicita su variabile con nome uguale a globale → MODIFICA la globale
    GlobalB = 777
    PRINT "Dentro TestImplicitModifiesOuter (implicita): GlobalB ="; GlobalB
END SUB

SUB TestImplicitCreatesNew()
    ' Assegnazione implicita su variabile INESISTENTE → crea nuova in scope corrente
    NewVar = 555
    PRINT "Dentro TestImplicitCreatesNew (implicita, nuova): NewVar ="; NewVar
END SUB

SUB TestLetCreatesNew()
    ' LET su variabile inesistente → crea nuova in scope corrente
    LET AnotherNew = 444
    PRINT "Dentro TestLetCreatesNew (LET, nuova): AnotherNew ="; AnotherNew
END SUB

SUB Main()
    CLS
    PRINT "=== Test LET vs Implicit Assignment ==="
    PRINT

    PRINT "1. Test LET shadowing:"
    PRINT "   Prima: GlobalA ="; GlobalA
    TestLetShadowing()
    PRINT "   Dopo SUB (globale): GlobalA ="; GlobalA
    PRINT

    PRINT "2. Test implicita modifica outer:"
    PRINT "   Prima: GlobalB ="; GlobalB
    TestImplicitModifiesOuter()
    PRINT "   Dopo SUB (globale modificata): GlobalB ="; GlobalB
    PRINT

    PRINT "3. Test implicita crea nuova:"
    TestImplicitCreatesNew()
    ' NewVar NON esiste qui (era locale alla SUB)
    PRINT

    PRINT "4. Test LET crea nuova:"
    TestLetCreatesNew()
    ' AnotherNew NON esiste qui (era locale alla SUB)
    PRINT

    PRINT "=== Fine test ==="
END SUB
