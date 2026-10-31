' MiniTest variabile il cui nome è una keyword (`Function`).
'
' Due difetti, stessa classe (dispatch per LEXEME sul nome del target invece che sul simbolo
' dichiarato), chiusi insieme:
'  - C99Native: l'emettore mandava ogni assegnamento a `Function` nel registro di ritorno
'    `__ret`, quindi `DIM Function = 5` + `Function = 7` lasciava la variabile a 0 — valore
'    SBAGLIATO in silenzio. Il parser produceva già l'AST corretto: la colpa era dell'emittore.
'    vedi Bugs/(solved) - C99Native-DIM-Function-Non-Dichiara.md
'  - emitter C#: il ritorno era emesso come `return` ANTICIPATO, così tutto il resto del corpo
'    diventava codice morto (le PRINT finali non giravano) e `FUNCTION = Function + 1` leggeva
'    il valore già sovrascritto. Ora scrive in uno slot e l'esecuzione prosegue, come gli altri.
'    vedi Bugs/(solved) - emitter C#-Function-Keyword-Return-Anticipato.md
'
' Regola di parità (misurata su IL):
'   - dentro un SUB `Function` è una normale variabile: 7 (nessuno slot di ritorno);
'   - dentro una FUNCTION `FUNCTION = <expr>` è il valore di ritorno: 42;
'   - dentro una FUNCTION l'inizializzatore del DIM inizializza la variabile (5) e
'     `Function = 7` va al ritorno lasciando la variabile a 5: `FUNCTION = Function + 1` = 6.
' EXPECT: 7|42|6
SUB Main()
    DIM Function = 5
    Function = 7
    PRINT LTRIM$(STR$(Function))
    PRINT LTRIM$(STR$(Raddoppia(21)))
    PRINT LTRIM$(STR$(ConVar()))
END SUB

FUNCTION Raddoppia(n AS DOUBLE) AS DOUBLE
    FUNCTION = n * 2
END FUNCTION

FUNCTION ConVar() AS DOUBLE
    DIM Function = 5
    Function = 7
    FUNCTION = Function + 1
END FUNCTION