' ORDER BY nella comprehension LETTERALE `[ out FOR v IN src [IF c] ORDER BY k [DESC|ASC] ]`
' (parità con la forma LINQ `FROM ... ORDER BY ... SELECT`). Supporta chiavi multiple
' separate da virgola. Prima la forma letterale non lo parsava ("Expected ']'...").
' EXPECT: asc= 1  2  3|desc= 3  2  1|s=Alice Bob Charlie
SUB Main()
    DIM nums = {3, 1, 2}
    DIM cresc = [ x FOR x IN nums ORDER BY x ]
    PRINT "asc="; cresc(0); cresc(1); cresc(2)
    DIM decresc = [ x FOR x IN nums ORDER BY x DESC ]
    PRINT "desc="; decresc(0); decresc(1); decresc(2)
    DIM nomi = {"Charlie", "Alice", "Bob"}
    DIM ord = [ n FOR n IN nomi ORDER BY n ]
    PRINT "s="; ord(0); " "; ord(1); " "; ord(2)
END SUB
