' Comprehension con SORGENTE STRINGA ({ n : LEN(n) FOR n IN names }, { k : "x" + k ... }, set { n FOR n IN names }):
' iteratore gmb_string_t (non double), chiave/valore stringa. Prima C99Native emetteva `gmb_string_t n = 0`
' (errore cc) dichiarava l'iteratore int; e la var dict restava a valore numerico (lettura NaN).
' EXPECT: lenBob= 3|ring=Ciao Alice|setCount= 3
SUB Main()
    DIM names = {"Alice", "Bob", "Charlie"}
    ' dict comprehension: chiave stringa, valore numerico
    DIM lens = { n : LEN(n) FOR n IN names }
    PRINT "lenBob="; lens("Bob")
    ' dict comprehension: chiave e valore stringa
    DIM ring = { n : "Ciao " + n FOR n IN names }
    PRINT "ring="; ring("Alice")
    ' set comprehension con sorgente stringa
    DIM s = { n FOR n IN names }
    PRINT "setCount="; s.Count
END SUB
