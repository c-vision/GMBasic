' NCM (FFI su libgmb_torch): senza i checkpoint LOADMODEL ritorna 0 e LASTERROR spiega perche'.
' La generazione vera (1 GB di pesi) e' nel sample AiLLM/NcmGenerate.bas.
' EXPECT: h= 0|err= 1
IMPORT NCM

SUB Main()
    DIM h AS LONG
    h = NCM.LOADMODEL("/nonexistent/generator.pt", "/nonexistent/tokenizer.pt", "/nonexistent/vocab.txt", 0)
    PRINT "h="; h
    DIM e AS STRING
    e = NCM.LASTERROR()
    PRINT "err="; IIF(LEN(e) > 0, 1, 0)
END SUB
