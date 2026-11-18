' FILE: Web/Mvc/Z1PolymorphicUpcast.bas
' =================================================================================================
'       Z1 — UPCAST DERIVATA→BASE + DISPATCH DINAMICO SU VARIABILE DIRETTA
' =================================================================================================
' Gap del linguaggio scoperto validando la Fase A del framework MVC (vedi
' , Fase Z — Z1):
'   1. Assegnare una classe derivata a una variabile del tipo base (o ritornarla da una
'      FUNCTION) falliva con "Impossibile assegnare un valore di tipo 'ClassType' a una
'      variabile di tipo 'X'" — il polimorfismo funzionava solo via elementi di array.
'   2. La chiamata di un metodo VIRTUAL su una variabile DIRETTA del tipo base invocava il
'      metodo BASE, non l'OVERRIDE (il dispatch dinamico scattava solo su elementi di array);
'      inoltre i FUNCTION method di classe non restituivano il loro valore (NIL/0).
'
' Caso target: la FUNCTION Json() ritorna un MvcJsonResult upcastato a IActionResult
' (derivata→base nel RETURN); Main lo assegna a una variabile del tipo base e chiama il
' metodo virtuale GetStatusCode() → deve stampare 200 (l'OVERRIDE), non 0 (il BASE).
' =================================================================================================

PUBLIC CLASS IActionResult
    PUBLIC VIRTUAL FUNCTION GetStatusCode() AS LONG
        GetStatusCode = 0
    END FUNCTION
END CLASS

PUBLIC CLASS MvcJsonResult INHERITS IActionResult
    PUBLIC DIM StatusCode AS LONG
    PUBLIC SUB NEW()
        SELF.StatusCode = 200
    END SUB
    PUBLIC OVERRIDE FUNCTION GetStatusCode() AS LONG
        GetStatusCode = SELF.StatusCode
    END FUNCTION
END CLASS

FUNCTION Json() AS IActionResult
    Json = NEW MvcJsonResult()
END FUNCTION

SUB Main()
    DIM r AS IActionResult = Json()
    PRINT r.GetStatusCode()
END SUB
