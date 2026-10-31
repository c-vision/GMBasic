' Letterale dict in un DIM tipizzato con tipi numerici NON double (INTEGER/LONG/SINGLE): il backend IL
' promuoveva a object (Dictionary<K,object>) e produceva IL non valido → crash del processo. Il letterale numerico va
' CONVERTITO nel tipo dichiarato.
' EXPECT: a= 1|b= 20|c= 3|f= 2.5
SUB Main()
    DIM a AS DICT OF STRING, INTEGER = {"x": 1, "y": 2}
    DIM b AS DICT OF STRING, LONG = {"p": 10, "q": 20}
    DIM c AS DICT OF STRING, DOUBLE = {"m": 3, "n": 4}
    DIM f AS DICT OF STRING, SINGLE = {"h": 2.5, "i": 7}
    PRINT "a="; a("x")
    PRINT "b="; b("q")
    PRINT "c="; c("m")
    PRINT "f="; f("h")
END SUB
