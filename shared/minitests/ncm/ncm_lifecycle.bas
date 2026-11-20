' NCM.LOADMODEL / FREEMODEL / LASTERROR: ciclo di vita degli handle con il modello minuscolo di samples/Fixtures/ncm_tiny.
' Due modelli vivi insieme hanno handle diversi e, con gli stessi pesi e lo stesso seed, producono lo stesso testo.
' Dopo FREEMODEL l'handle non e' piu' valido: GENERATE torna "" e LASTERROR spiega perche'. Liberare due volte o liberare
' l'handle 0 non fa nulla di grave; un numero di campioni non positivo e' un errore dichiarato, non un crash.
' EXPECT: distinct= 1|equal= 1|freed= 1|err1= 1|alive= 1|double=1|zero= 1|err2= 1
IMPORT NCM

SUB Main()
    DIM FX AS STRING
    FX = "../../samples/Fixtures/ncm_tiny/"
    DIM h1 AS LONG, h2 AS LONG
    h1 = NCM.LOADMODEL(FX + "generator.pt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    h2 = NCM.LOADMODEL(FX + "generator.pt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    PRINT "distinct="; IIF(h1 > 0 AND h2 > 0 AND h1 <> h2, 1, 0)

    DIM a AS STRING, b AS STRING
    a = NCM.GENERATE(h1, 2, 5)
    b = NCM.GENERATE(h2, 2, 5)
    PRINT "equal="; IIF(LEN(a) > 0 AND a = b, 1, 0)

    NCM.FREEMODEL(h1)
    DIM dead AS STRING
    dead = NCM.GENERATE(h1, 2, 5)
    PRINT "freed="; IIF(LEN(dead) = 0, 1, 0)
    PRINT "err1="; IIF(LEN(NCM.LASTERROR()) > 0, 1, 0)

    ' l'altro modello non e' toccato
    PRINT "alive="; IIF(NCM.GENERATE(h2, 2, 5) = a, 1, 0)

    NCM.FREEMODEL(h1)
    NCM.FREEMODEL(0)
    PRINT "double=1"

    DIM none AS STRING
    none = NCM.GENERATE(h2, 0, 1)
    PRINT "zero="; IIF(LEN(none) = 0, 1, 0)
    PRINT "err2="; IIF(LEN(NCM.LASTERROR()) > 0, 1, 0)

    NCM.FREEMODEL(h2)
END SUB
