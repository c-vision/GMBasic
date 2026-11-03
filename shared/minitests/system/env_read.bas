' MiniTest ENV — ENV$(nome) e ENV.HAS(nome), sola lettura, 4 backend.
' Copre: argomento letterale e VARIABILE, PATH presente, variabile assente,
' nome VUOTO (ENV$="" / ENV.HAS=0), lunghezza > 0.
' EXPECT: present=-1|missing= 0|emptyhas= 0|emptyval=[]|lenvar=-1
IMPORT ENV

SUB Main()
    DIM name AS STRING
    name = "PATH"
    PRINT "present="; ENV.HAS(name)
    PRINT "missing="; ENV.HAS("GMB_DEFINITELY_NOT_SET_123")
    PRINT "emptyhas="; ENV.HAS("")
    PRINT "emptyval=["; ENV$(""); "]"
    PRINT "lenvar="; (LEN(ENV$(name)) > 0)
END SUB
