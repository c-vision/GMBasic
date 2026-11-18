' MiniTest CONSTRUCTS — contenitore dichiarato a livello di MODULO senza inizializzatore.
'
' `DIM d AS DICT` (globale) deve essere "pronto all'uso" come il corrispettivo locale:
' la prima scrittura `d("k") = v` non deve trovare null. Sul backend IL il globale senza
' inizializzatore restava null (EmitDefaultValue su un tipo reference) → NullReferenceException
' alla prima scrittura (chiuso il 2026-11-13: le collezioni BASIC si materializzano anche
' nel ramo GLOBALE, come già faceva il ramo LOCALE).
'
' EXPECT: d= 2 |n= 2 |l= 10
DIM d AS DICT
DIM l AS LIST

SUB Main()
    d("k") = 2
    PRINT "d="; d("k");

    d("j") = 5
    PRINT "|n="; LEN(d);

    l.Add(10)
    PRINT "|l="; l(0)
END SUB
