' Lettura elemento di un val JSON/dict boxed (`j("k")`) in contesto STRINGA e in scrittura su un
' altro JSON: su C99Native era classificata TY_NUM → gmb_make_val_double((double)(val)) /
' gmb_double_to_string(val) = errore cc (bug BspModelAuthDemo). Ora e' TY_VAL (val grezzo).
' EXPECT: u=Alice|w=Benvenuto Alice
SUB Main()
    DIM j AS JSON = JSON.Parse("{""name"":""Alice"",""role"":""admin""}")
    DIM s AS JSON = NEW JSON
    s("user") = j("name")
    PRINT "u="; s("user")
    PRINT "w="; "Benvenuto " + j("name")
END SUB
