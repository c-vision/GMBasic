' MiniTest SYSTEM.ENV$ / SYSTEM.HAS — ambiente di processo (sola lettura).
' ENV$/HAS vivono in SYSTEM, sempre attivo: NESSUN IMPORT necessario.
' Copre: argomento letterale e VARIABILE, PATH presente, variabile assente,
' nome VUOTO (ENV$="" / SYSTEM.HAS=0), lunghezza > 0.
' EXPECT: present=-1|missing= 0|emptyhas= 0|emptyval=[]|lenvar=-1
SUB Main()
    DIM name AS STRING
    name = "PATH"
    PRINT "present="; SYSTEM.HAS(name)
    PRINT "missing="; SYSTEM.HAS("GMB_DEFINITELY_NOT_SET_123")
    PRINT "emptyhas="; SYSTEM.HAS("")
    PRINT "emptyval=["; ENV$(""); "]"
    PRINT "lenvar="; (LEN(ENV$(name)) > 0)
END SUB
