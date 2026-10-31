' EXPECT: Error: 0 An invalid request URI was provided. Either the request URI must be an absolute URI or BaseAddress must be set.
IMPORT AI

SUB Main()
    PRINT AI.INGESTWEB("x", "x", "x")
END SUB
