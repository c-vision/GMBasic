' EXPECT: [ 2500]|[-7]|[ 3.5]|[ 2500]|[ 3.5]
IMPORT CONVERSIONS
SUB Main()
    PRINT "["; STR$(2500); "]"
    PRINT "["; STR$(-7); "]"
    PRINT "["; STR$(3.5); "]"
    PRINT "["; CSTR(2500); "]"
    PRINT "["; CSTR(3.5); "]"
END SUB
