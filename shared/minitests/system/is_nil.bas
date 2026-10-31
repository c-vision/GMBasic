' EXPECT: 0|0|0
' IS NIL con le STRINGHE (e un caso numerico). La stringa è NIL quando il riferimento è nullo:
' - stringa valorizzata -> 0
' - stringa VUOTA -> 0 (una stringa vuota NON è NIL: ha comunque un'istanza)
' - variabile numerica non assegnata/assente -> NIL (-1)
' Copre il bug di emissione nativa corretto: `x$ IS NIL` era tradotto in
' `isnan((gmb_double_t)(x$))` -> C non compilabile ("operand of type 'gmb_string_t' ...").
IMPORT SYSTEM

SUB Main()
    s$ = "valore"
    PRINT s$ IS NIL
    s$ = ""
    PRINT s$ IS NIL
    DIM n AS DOUBLE
    PRINT n IS NIL
END SUB
