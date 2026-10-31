' FILE: Clipper/ClipperMacroCanary.bas
' =================================================================================================
'   CANARINO — macro & Clipper, entrambe le forme: STATEMENT (&(cmd)) ed ESPRESSIONE (&expr come
'   valore), con cattura variabili globali. Vedi
'   AiWiki/01-Projects/GMBASIC/Dynamic-Code-EVAL-Macro-Backends-Report.md per il contesto completo.
'
'   Prima dei fix (sessione 2026-08-21):
'   - &(cmd) sotto NativeAOT (IL Backend, build reale) lanciava sempre un'eccezione
'     (IlRuntime.MacroEvalPCode era un throw NotSupportedException incondizionato);
'   - &expr come VALORE (es. y = &("2+3")) era un passthrough puro SIA sotto JIT SIA sotto
'     NativeAOT: valutava l'espressione INTERNA e ne ritornava il valore, mai il risultato del
'     testo compilato — y = &("2+3") assegnava "2+3" (stringa), mai 5;
'   - in entrambi i casi, senza la cattura delle variabili GLOBALI la macro avrebbe visto le
'     variabili del programma come NIL/non definite invece del loro valore reale (il Compiler
'     P-Code compila la macro in isolamento, _globals parte vuoto).
'
'   EVAL(expr) è sintassi zucchero per &expr (stesso identico nodo AST MacroExpr, aggiunta
'   subito dopo — vedi Parser.Expressions.cs), quindi eredita automaticamente lo stesso fix.
'
'   NOTA (scoperta verificando dal vivo il fix write-persistence, sessione 2026-08-21):
'   l'ASSEGNAZIONE X = 10 è dentro SUB Main(), non a livello di modulo — quando esiste una SUB
'   Main(), le ISTRUZIONI eseguibili a livello di modulo (fuori da qualunque SUB) NON vengono
'   eseguite prima di essa (verificato con un PRINT X diretto, non-macro: stampava 0 nonostante
'   "X = 10" precedente a livello di modulo). La DICHIARAZIONE GLOBAL X AS INTEGER invece resta
'   a livello di modulo (le dichiarazioni vengono comunque elaborate, è solo l'assegnazione
'   runtime a non eseguire). Comportamento preesistente del linker/entry-point, indipendente da
'   questa sessione — non è il bug qui testato, ma un'inizializzazione a livello di modulo
'   avrebbe reso questo canarino inaffidabile per isolare il bug macro/EVAL.
'
'   Output atteso (verificato dal vivo il 2026-08-21 con Executor -n):
'     test1-simple: 42
'     test2-read-X: 10
'     test3-write-X: 15
'     test3-write-ok
'     test4-arith: 5
'     test5-expr-arith: 5
'     test6-expr-read-X: 15
'     test7-eval-arith: 5
'     test8-eval-read-X: 15
'     test9-eval-cmd-X: 99
'     test9-write-ok
' =================================================================================================

GLOBAL X AS INTEGER

SUB Main()
    X = 10

    ' 1. Macro statement autosufficiente, nessuna variabile esterna coinvolta.
    PRINT "test1-simple:";
    &("PRINT 42")

    ' 2. Macro statement che LEGGE una variabile globale REALE del programma.
    '    Atteso: 10 (non NIL/0 — la X letta dalla macro e' la stessa X dichiarata sopra).
    PRINT "test2-read-X:";
    &("PRINT X")

    ' 3. Macro statement che SCRIVE una variabile globale REALE del programma.
    '    Atteso: dopo la macro X vale 15 anche QUI, non solo dentro la macro stessa.
    &("X = X + 5")
    PRINT "test3-write-X:"; X
    PRINT "test3-write-"; IIF(X = 15, "ok", "FAIL")

    ' 4. Macro statement con espressione aritmetica pura (nessuna variabile).
    PRINT "test4-arith:";
    &("PRINT 2 + 3")

    ' 5. Macro ESPRESSIONE (valore): il risultato del testo compilato, non la stringa.
    '    Atteso: 5 (numero), non la stringa "2+3".
    DIM y AS INTEGER
    y = &("2+3")
    PRINT "test5-expr-arith:"; y

    ' 6. Macro espressione che LEGGE una variabile globale REALE (X vale 15 a questo punto).
    DIM z AS INTEGER
    z = &("X")
    PRINT "test6-expr-read-X:"; z

    ' 7. EVAL(expr) — stessa cosa di &expr, sintassi con parentesi obbligatorie.
    DIM v AS INTEGER
    v = EVAL("2+3")
    PRINT "test7-eval-arith:"; v

    ' 8. EVAL(expr) che legge una variabile globale REALE (X vale 15 a questo punto).
    DIM u AS INTEGER
    u = EVAL("X")
    PRINT "test8-eval-read-X:"; u

    ' 9. EVAL con un testo che NON è un'espressione valida ma un COMANDO (un'assegnazione) —
    '    deve eseguirlo comunque, non fallire con un errore di parsing. Il valore di ritorno
    '    di EVAL qui non è significativo (NIL), quello che conta è l'effetto su X.
    DIM dummy AS INTEGER
    dummy = EVAL("X = 99")
    PRINT "test9-eval-cmd-X:"; X
    PRINT "test9-write-"; IIF(X = 99, "ok", "FAIL")
END SUB
