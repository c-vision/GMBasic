' EXPECT: Alice|30|Alice|30|Alice|30
' MiniTest: elementi STRING in una tupla nominata
' Copre accesso per nome (person.name), per indice (person(0)) e destrutturazione
' (DIM (n, a) = person) — i backend devono preservare il tipo stringa (non coercizione
' a double). Parità 5/5.
IMPORT CONSOLE
SUB Main()
    DIM person = (name: "Alice", age: 30)
    PRINT person.name
    PRINT person.age
    PRINT person(0)
    PRINT person(1)
    DIM (n, a) = person
    PRINT n
    PRINT a
END SUB
