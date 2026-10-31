' FILE: Clipper/ClipperMacroEvalBlocksExtreme.bas
' ======================================================================================================================
' CLIPPER 5.3 — MACRO & , EVAL, CODE BLOCK `{ |p| ... }` — sample ESTREMO/complesso
' ======================================================================================================================
' Semantica Clipper 5.3 (Guide To CA-Clipper):
'   &<cVar>[.]      macro VARIABILE: compila+esegue il contenuto della variabile carattere.
'   &(<cExpr>)      macro ESPRESSIONE: valuta cExpr, compila+esegue la stringa risultante.
'   "testo &cVar"   text substitution nelle stringhe (variabili PRIVATE/PUBLIC = globali/modulo;
'                   local/static/field/elemento restano testo letterale).
'   macro annidate: &cOne con cOne contenente "&cTwo" ecc.
'   &("{ |x| ... }")-> bBlock   macro che produce un CODE BLOCK (compilato una volta).
'   EVAL(bBlock [, args...])    esegue un code block con argomenti.
'   AEVAL(aArr, bBlock)         il block riceve (elemento, indice).
' GMBASIC (piu' flessibile, non meno): EVAL("stringa") resta il macro-evaluator di stringa.
'
' NOTA (2026-10-02): la macro su VARIABILE con contenuto LETTERALE (DIM s = "{ |p| ... }")
' e' espansa a COMPILE-TIME come la forma costante (sorgente unica: Linker RewriteMacroExpr,
' commit 545b782e6): tutti e 5 i backend producono la stessa lambda tipizzata. Stringhe
' costruite DINAMICAMENTE a runtime restano non espandibili sui transpiler C (residuo
' documentato nel bug).
' ======================================================================================================================
IMPORT ARRAYS

GLOBAL TaxRate AS DOUBLE
GLOBAL cName AS STRING

FUNCTION MkInc() AS FUNCTION(x) AS DOUBLE
    RETURN { |x| RETURN x + 1 }
END FUNCTION

FUNCTION ApplyFn(b AS FUNCTION(x) AS DOUBLE, v AS DOUBLE) AS DOUBLE
    RETURN EVAL(b, v)
END FUNCTION

SUB Main()
    TaxRate = 0.22
    PRINT "=== Clipper macro/EVAL/blocks (extreme) ==="

    ' --- 1. Macro ESPRESSIONE compilata (aritmetica + globale) ---
    DIM cPrice AS STRING = "100"
    DIM cNet AS STRING = "VAL(cPrice) * (1 + TaxRate)"
    PRINT "net="; &(cNet)                       ' 122

    ' --- 2. Macro VARIABILE: il nome della variabile e' nei dati ---
    DIM which AS STRING = "TaxRate"
    PRINT "tax="; &which                        ' 0.22

    ' --- 3. Macro COSTANTE che produce un CODE BLOCK (compilato una volta) ---
    DIM bGross = &("{ |p| RETURN p * (1 + TaxRate) }")
    PRINT "gross(100)="; EVAL(bGross, 100)      ' 122
    PRINT "gross(200)="; EVAL(bGross, 200)      ' 244

    ' --- 3b. Macro su VARIABILE con block LETTERALE (espansione compile-time) ---
    DIM cBlockDef AS STRING = "{ |p| RETURN p * (1 + TaxRate) }"
    DIM bVar = &(cBlockDef)
    PRINT "vargross(150)="; EVAL(bVar, 150)     ' 183

    ' --- 4. Code block letterale + EVAL multi-argomento ---
    DIM bArea = { |w, h| RETURN w * h }
    PRINT "area="; EVAL(bArea, 6, 7)            ' 42

    ' --- 4b. Block con concat: risultato stringa + param usato solo in concat ---
    DIM bBang = { |s| RETURN UPPER(s) + "!" }
    PRINT "bang="; EVAL(bBang, "x")             ' X!
    DIM bRaw = { |s| RETURN s + "!" }
    PRINT "raw="; EVAL(bRaw, "x")               ' x!

    ' --- 5. Block applicato a tutti gli elementi di un array (AEVAL: elemento, indice) ---
    DIM prices = {100, 200, 300}
    AEVAL prices, { |v, i| PRINT "  gross["; i; "]="; v * (1 + TaxRate) }   ' 122 244 366

    ' --- 6. Macro annidate ---
    DIM cThree AS STRING = "deep!"
    DIM cTwo AS STRING = "cThree"
    DIM cOne AS STRING = "&cTwo"
    PRINT "nested="; &cOne                       ' deep!

    ' --- 7. EVAL stringa (GMBasic) come COMANDO + text substitution (globale) ---
    EVAL("TaxRate = 0.10")
    PRINT "tax2="; &("TaxRate")                  ' 0.1
    cName = "world"
    PRINT "subst=Hello &cName!"                  ' Hello world!  (globale espanso)

    ' --- 8. EVAL con block INLINE + block restituito da funzione ---
    PRINT "inline="; EVAL({ |s| RETURN UPPER(s) + "!" }, "x")   ' X!
    PRINT "fromfn="; EVAL(MkInc(), 5)                           ' 6
    DIM bm = MkInc()
    PRINT "infvar="; EVAL(bm, 5)                                ' 6
    EVAL({ |s| PRINT "stmt="; s }, "y")                         ' stmt=y

    ' --- 9. Closure: EVAL annidato + function-value catturato + param FUNCTION ---
    PRINT "nested="; EVAL({ |x| RETURN EVAL({ |y| RETURN y + 1 }, x) }, 5)  ' 6
    DIM inner = { |y| RETURN y + 1 }
    DIM outer = { |x| RETURN EVAL(inner, x) }
    PRINT "capt="; EVAL(outer, 5)                               ' 6
    PRINT "param="; ApplyFn({ |z| RETURN z * 2 }, 21)           ' 42

    PRINT "Clipper macro/EVAL/blocks: fatto."
END SUB
