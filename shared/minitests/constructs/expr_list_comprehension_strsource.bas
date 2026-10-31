' Comprehension LISTA con SORGENTE STRINGA ([ n FOR n IN names ]): elemento stringa, array di
' stringhe (non double). Prima IL generava List<double>/double[] (InvalidProgram) e C99Native
' `gmb_make_val_double(n)` (errore cc); emitter C# convertiva con gmb_list_to_ints_array.
' EXPECT: n0=Charlie|n2=Bob|len= 2|f1=Alice|flen= 1
SUB Main()
    DIM names = {"Charlie", "Alice", "Bob"}
    DIM up = [ n FOR n IN names ]
    PRINT "n0="; up(0)
    PRINT "n2="; up(2)
    PRINT "len="; UBOUND(up)
    DIM filt = [ n FOR n IN names IF n <> "Bob" ]
    PRINT "f1="; filt(1)
    PRINT "flen="; UBOUND(filt)
END SUB
