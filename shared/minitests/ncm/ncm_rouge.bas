' NCM.ROUGE1 / ROUGE2 / ROUGEL: F-measure fra riferimento e ipotesi, senza modelli (funzione pura dell'host torch).
' Valori calcolati a mano: "the cat sat" contro "the cat sat on the mat" -> ROUGE-1 P=3/6 R=1 F=2/3; ROUGE-2 (2 bigrammi su 5) P=2/5 R=1 F=4/7;
' ROUGE-L (LCS = 3) F=2/3. Lo stemmer di Porter porta running e runs a "run"; maiuscole e punteggiatura non contano; testo vuoto = 0.
' Gli scarti dello stemmer rispetto a NLTK non toccano questi casi. Valori scritti x 10000 e arrotondati.
' EXPECT: r1= 6667|r2= 5714|rl= 6667|stem= 6667|norm= 10000|same= 10000|empty= 0|sym= 1|order= 5000
IMPORT NCM

FUNCTION Q(x AS DOUBLE) AS LONG
    RETURN INT(x * 10000 + 0.5)
END FUNCTION

SUB Main()
    DIM ref AS STRING, hyp AS STRING
    ref = "the cat sat"
    hyp = "the cat sat on the mat"
    PRINT "r1="; Q(NCM.ROUGE1(ref, hyp))
    PRINT "r2="; Q(NCM.ROUGE2(ref, hyp))
    PRINT "rl="; Q(NCM.ROUGEL(ref, hyp))
    PRINT "stem="; Q(NCM.ROUGE1("run", "running runs"))
    PRINT "norm="; Q(NCM.ROUGE1("Hello, World!", "hello   world"))
    PRINT "same="; Q(NCM.ROUGE2(hyp, hyp))
    PRINT "empty="; Q(NCM.ROUGE1("", "x"))
    PRINT "sym="; IIF(Q(NCM.ROUGE1(ref, hyp)) = Q(NCM.ROUGE1(hyp, ref)), 1, 0)
    ' ROUGE-L tiene conto dell'ordine, ROUGE-1 no: "a b" contro "b a" -> R1 = 1, RL = 1/2
    PRINT "order="; Q(NCM.ROUGEL("a b", "b a"))
END SUB
