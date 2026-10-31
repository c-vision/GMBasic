' EXPECT: dict-4= 16|dict-5= 25
SUB Main()
    DIM dict_quadrati AS DICT OF DOUBLE, DOUBLE = { x : x*x FOR x IN {1, 2, 3, 4} }
    PRINT "dict-4="; dict_quadrati("4")
    DIM dict_dispari AS DICT OF DOUBLE, DOUBLE = { x : x*x FOR x IN {1, 2, 3, 4, 5} IF x MOD 2 <> 0 }
    PRINT "dict-5="; dict_dispari("5")
END SUB
