' EXPECT: COMPILE ERROR: Il simbolo 'pippo' non è definito. Con OPTION STRICT ON, tutte le variabili devono essere dichiarate esplicitamente.
' MiniTest NEGATIVO di compilazione: OPTION STRICT ON = «nessuna variabile implicita». Un nome
' MAI dichiarato è un errore di compilazione, con lo stesso messaggio sulle 4 gambe
' (stesso testo del linker C#: ErrorCode.UndefinedVariable). Fuori da STRICT lo stesso
' programma compilerebbe e stamperebbe 0 (variabile implicita, manuale II.5 §1).
OPTION STRICT ON

SUB Main()
    PRINT "pippo="; pippo
END SUB
