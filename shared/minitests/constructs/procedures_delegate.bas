' EXPECT: res= 7|sub= 10
' Sintassi canone: samples/Functions/FunctionsDelegates.bas
DELEGATE FUNCTION OperationDelegate(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
FUNCTION AddNumbers(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a + b
END FUNCTION
SUB Apply(o AS OperationDelegate, x AS DOUBLE, y AS DOUBLE)
    PRINT "res="; o(x, y)
END SUB
SUB Main()
    Apply(AddNumbers, 3, 4)
    DIM op AS OperationDelegate = AddNumbers
    PRINT "sub="; op(10, 0)
END SUB
