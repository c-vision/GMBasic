' FILE: Web/Mvc/Z3InheritsQualified/HttpController.inc
' =================================================================================================
'       Z3 — CLASSE BASE IN UN ALTRO FILE CON NAMESPACE
' =================================================================================================
' Modulo .inc in sottocartella con NAMESPACE MVC: importabile con `IMPORT MVC` e usato come
' base qualificata `INHERITS MVC.HttpController` da una classe definita nel file cliente.
' Gap Z3 del piano MVC (, Fase Z):
' INHERITS prendeva un singolo Token (nome nudo) e la base doveva stare nello stesso file di
' parsing — con la base in un .inc della sottocartella o qualificata il parse falliva
' ("Prevista una definizione di membro" / "Expected a newline after CLASS declaration").
' =================================================================================================

NAMESPACE MVC

PUBLIC CLASS HttpController
    PUBLIC SUB Json(res AS HTTPRESPONSE, data AS ANY)
        res.SendJSON(data)
    END SUB
END CLASS
