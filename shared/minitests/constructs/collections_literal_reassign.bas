' MiniTest CONSTRUCTS — RIASSEGNAZIONE di un contenitore con un LETTERALE fuori dal DIM.
'
' Copre due difetti dell'emitter C99 (chiusi il 2026-11-13):
'   - `l = [1,2,3]` su una variabile LIST emetteva il percorso ARRAY nativo
'     (`gmb_array_resize((void**)&l, &l_len, …)` su un gmb_list_t) → C che non compilava;
'   - `d = {"a": 1}` su una variabile DICT assegnava un gmb_val_t a un gmb_dict_t.
' La dichiarazione (`DIM x AS LIST/DICT … = [..]/{..}`) aveva già i rami giusti: mancava
' la forma in ASSEGNAMENTO.
'
' EXPECT: len= 3 |l1= 2 |len2= 2 |l1b= 8 |d= 1 |d2= 2
IMPORT COLLECTIONS

DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER
DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER

SUB Main()
    l = [1, 2, 3]
    PRINT "len="; LEN(l); "|l1="; l(1);

    ' riassegnazione con un ALTRO letterale (sostituisce il contenuto)
    l = [7, 8]
    PRINT "|len2="; LEN(l); "|l1b="; l(1);

    d = {"a": 1}
    PRINT "|d="; d("a");

    ' e con un letterale di più coppie
    d = {"a": 2, "b": 3}
    PRINT "|d2="; d("a")
END SUB
