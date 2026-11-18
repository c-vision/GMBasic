' EXPECT: COMPILE ERROR: LET è valido solo dentro una procedura (SUB/FUNCTION/GENERATOR): a livello di modulo usare un'assegnazione (x = ...) oppure DIM/GLOBAL.
' MiniTest NEGATIVO di compilazione: spec Variables-Scope §3.1, ultima riga — «LET è valido solo
' all'interno di procedure o blocchi; a livello di Namespace genera un errore di sintassi».
' A livello di modulo il corpo del programma NON è una procedura: LET non ha uno «scope corrente»
' più interno di quello di modulo, quindi è un errore (stesso testo sul parser C# e sul parser C).
LET x = 5

SUB Main()
    PRINT "x="; x
END SUB
