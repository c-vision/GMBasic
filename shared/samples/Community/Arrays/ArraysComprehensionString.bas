' FILE: Arrays/ArraysComprehensionString.bas
' =================================================================================================
'   COMPRENSIONI con SORGENTE STRINGA (LIST / DICT / SET)
' =================================================================================================
' Le comprehension accettano anche sorgenti di STRINGHE: l'iteratore è una stringa, l'array
' risultante è di stringhe, e chiavi/valori dict possono essere stringhe.
'
'   [ <expr> FOR s IN <coll> ]        -> array di stringhe
'   { s : <expr> FOR s IN <coll> }    -> DICT con chiave stringa
'   { s FOR s IN <coll> }             -> HASHSET di stringhe (dedup)
'   [ s FOR s IN {"a","b"} IF ... ]   -> sorgente LETTERALE di stringhe + filtro
'
' Componenti: <expr> espressione output; s iteratore (stringa); <coll> sorgente (array/lista/
' letterale di stringhe); IF <cond> filtro opzionale.
' =================================================================================================

IMPORT ARRAYS
IMPORT COLLECTIONS

SUB Main()
    DIM nomi = {"Charlie", "Alice", "Bob"}

    PRINT "--- 1. List comprehension da array di stringhe ---"
    DIM copia = [ nome FOR nome IN nomi ]
    PRINT "copia0="; copia(0); " copia2="; copia(2); " ubound="; UBOUND(copia)

    PRINT "--- 2. List comprehension con filtro (LEN <= 5) ---"
    DIM corti = [ nome FOR nome IN nomi IF LEN(nome) <= 5 ]
    PRINT "corti0="; corti(0); " corti1="; corti(1); " ubound="; UBOUND(corti)

    PRINT "--- 3. List comprehension su LETTERALE di stringhe ---"
    DIM up = [ UPPER(s) FOR s IN {"a", "b", "c"} ]
    PRINT "up0="; up(0); " up1="; up(1); " up2="; up(2); " ubound="; UBOUND(up)

    PRINT "--- 4. Dict comprehension: chiave STRINGA -> valore numerico ---"
    DIM lunghezze = { nome : LEN(nome) FOR nome IN nomi }
    PRINT "alice="; lunghezze("Alice"); " bob="; lunghezze("Bob"); " charlie="; lunghezze("Charlie")

    PRINT "--- 5. Dict comprehension: chiave STRINGA -> valore STRINGA ---"
    DIM saluti = { nome : "Ciao " + nome FOR nome IN nomi }
    PRINT "bob="; saluti("Bob")

    PRINT "--- 6. Set comprehension di stringhe (dedup) ---"
    DIM distinti = { s FOR s IN {"a", "b", "a", "c", "b"} }
    PRINT "distinti="; distinti.Count

    PRINT "Fatto."
END SUB
