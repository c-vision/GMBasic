' MiniTest — TYPENAME/VALTYPE delle collezioni (parità 4 backend).
' I nomi sono quelli canonici CLR (IlRuntime.TypeName/golden C99): SortedDictionary/Tree/
' AVL/RedBlack/BITree/BPlusTree → "SORTEDDICTIONARY`2"; HashSet NON implementa ICollection
' non-generico → VALTYPE "O". Prima: PCode/VM-C davano nomi diversi e i backend C non
' sapevano boxare/emettere (C invalido) per VALTYPE(dict/list).
' EXPECT: d=DICTIONARY`2/A|l=LIST`1/A|q=QUEUE`1/A|s=STACK`1/A|so=SORTEDDICTIONARY`2/A|hs=HASHSET`1/O|trie=TRIE/O|bag=DICTIONARY`2/A|cache=CACHE`2/O
IMPORT COLLECTIONS
IMPORT SYSTEM

SUB Main()
    DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
    DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER
    DIM q AS QUEUE OF STRING = NEW QUEUE OF STRING
    DIM s AS STACK OF LONG = NEW STACK OF LONG
    DIM so AS SORTED OF STRING, INTEGER = NEW SORTED OF STRING, INTEGER
    DIM hs AS HASHSET OF INTEGER = [1, 2, 3]
    DIM tr AS TRIE = ["car", "cat"]
    DIM bg AS BAG<STRING> = ["a", "b"]
    DIM ca AS CACHE<STRING, STRING>

    PRINT "d="; TYPENAME(d); "/"; SYSTEM.VALTYPE(d)
    PRINT "l="; TYPENAME(l); "/"; SYSTEM.VALTYPE(l)
    PRINT "q="; TYPENAME(q); "/"; SYSTEM.VALTYPE(q)
    PRINT "s="; TYPENAME(s); "/"; SYSTEM.VALTYPE(s)
    PRINT "so="; TYPENAME(so); "/"; SYSTEM.VALTYPE(so)
    PRINT "hs="; TYPENAME(hs); "/"; SYSTEM.VALTYPE(hs)
    PRINT "trie="; TYPENAME(tr); "/"; SYSTEM.VALTYPE(tr)
    PRINT "bag="; TYPENAME(bg); "/"; SYSTEM.VALTYPE(bg)
    PRINT "cache="; TYPENAME(ca); "/"; SYSTEM.VALTYPE(ca)
END SUB
