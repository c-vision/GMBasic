' ForEachJsonEdgeCases.bas — casi limite FOR EACH + JSON (mondo C / parità backend)
' Verifica: RANGE→LEN/ForEach, dict coppie k,v, KeyValuePair, stringhe, matrici,
' JSON parse/EXTRACT/nested foreach, WRITEVALUE/ReadValue.
IMPORT JSON

SUB Main()
    CLS
    PRINT "=== ForEach + JSON edge suite ==="

    ' 1. RANGE come lista dinamica: LEN + FOR EACH
    DIM r = RANGE(1, 5)
    DIM s = 0 : DIM c = 0
    FOR EACH v IN r
        s = s + v : c = c + 1
    NEXT
    IF s = 15 AND c = 5 AND r(4) = 5 THEN PRINT "range OK" ELSE PRINT "range FAIL"

    ' 2. DICT da JSON: FOR EACH k,v (coppie)
    DIM d = JSON.Parse("{ ""a"": 1, ""b"": 2, ""c"": 3 }")
    DIM ks = 0 : DIM vs = 0
    FOR EACH k, v IN d
        ks = ks + 1 : vs = vs + v
    NEXT
    IF ks = 3 AND vs = 6 THEN PRINT "dict-pairs OK" ELSE PRINT "dict-pairs FAIL"

    ' 3. DICT: FOR EACH entry (KeyValuePair) con .Value
    DIM kv = 0
    FOR EACH e IN d
        kv = kv + e.Value
    NEXT
    IF kv = 6 THEN PRINT "kvp OK" ELSE PRINT "kvp FAIL"

    ' 4. STRING: FOR EACH char
    DIM cc = 0
    FOR EACH ch IN "abc"
        cc = cc + 1
    NEXT
    IF cc = 3 THEN PRINT "string OK" ELSE PRINT "string FAIL"

    ' 5. MATRICE 2D: FOR EACH row
    DIM m(1, 1) AS INTEGER
    m(0,0) = 1 : m(0,1) = 2 : m(1,0) = 3 : m(1,1) = 4
    DIM rr = 0
    FOR EACH row IN m
        rr = rr + 1
    NEXT
    IF rr = 2 THEN PRINT "matrix OK" ELSE PRINT "matrix FAIL"

    ' 6. JSON nested: EXTRACT + ForEach sull'array annidato
    DIM j = JSON.Parse("{ ""user"": ""Mario"", ""roles"": [""admin"", ""editor""] }")
    IF JSON.EXTRACT(j, "roles[1]") = "editor" THEN PRINT "extract OK" ELSE PRINT "extract FAIL"
    DIM ra = j.roles
    DIM rn = 0
    FOR EACH x IN ra
        rn = rn + 1
    NEXT
    IF rn = 2 THEN PRINT "nested-foreach OK" ELSE PRINT "nested-foreach FAIL"

    ' 7. WRITEVALUE/ReadValue (documento impostazioni in-memory)
    JSON WRITEVALUE "k1", "v1"
    IF JSON.ReadValue("k1") = "v1" THEN PRINT "wrv OK" ELSE PRINT "wrv FAIL"

    PRINT "=== done ==="
END SUB
