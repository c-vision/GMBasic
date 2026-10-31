' Oracle = golden: INTERFACE + UDT IMPLEMENTS (chiamata di metodo via interfaccia).
' Sintassi canone: samples/Types/TypesImplementsInterface.bas
' 2026-09-21: FIX IL/PCode(+VM-C, stesso bytecode condiviso) — root cause reale: NON era
' dispatch via interfaccia (riproducibile anche su un TYPE senza alcuna INTERFACE), ma la
' convenzione di ritorno "bare-assignment NomeMetodo = espr" mai wired per i metodi di TYPE
' (mancava per IL l'argomento returnSymbolId del costruttore, per PCode/VM-C il contesto
' _currentFunctionReturnType/_currentProcedureName — stesso fix "Z1" già presente per i
' metodi di CLASS, mai portato ai metodi di TYPE). Vedi AiWiki "(solved) -
' UDT-Interface-Dispatch-Default-Su-Gambe-CSharp.md". emitter C#: gap SEPARATO e più
' profondo scoperto verificando questo fix (la chiamata r.Area() non viene proprio emessa,
' PRINT stampa l'intero oggetto r) — NON risolto, documentato in
' "(open) - emitter C#-UDT-Method-Call-Non-Emesso.md". C99Native corretto.
' EXPECT: area= 50|shape=Rect
INTERFACE IShape
    FUNCTION Area() AS DOUBLE
    FUNCTION Describe() AS STRING
END INTERFACE

TYPE Rettangolo IMPLEMENTS IShape
    Larghezza AS DOUBLE
    Altezza AS DOUBLE
    FUNCTION Area() AS DOUBLE
        AREA = SELF.Larghezza * SELF.Altezza
    END FUNCTION
    FUNCTION Describe() AS STRING
        DESCRIBE = "Rect"
    END FUNCTION
END TYPE

SUB Main()
    DIM r AS Rettangolo
    r.Larghezza = 10 : r.Altezza = 5
    PRINT "area="; r.Area()
    PRINT "shape="; r.Describe()
END SUB
