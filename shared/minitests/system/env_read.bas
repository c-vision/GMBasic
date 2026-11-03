' MiniTest ENV — ENV$(nome) e ENV.HAS(nome), sola lettura, 4 backend.
' PATH è presente in ogni processo; una variabile inesistente dà "" / 0.
' EXPECT: haspath=-1|lenpath=-1|nope= 0|noval=[]
IMPORT ENV

SUB Main()
    PRINT "haspath="; ENV.HAS("PATH")
    PRINT "lenpath="; (LEN(ENV$("PATH")) > 0)
    PRINT "nope="; ENV.HAS("GMB_DEFINITELY_NOT_SET_123")
    PRINT "noval=["; ENV$("GMB_DEFINITELY_NOT_SET_123"); "]"
END SUB
