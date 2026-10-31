' FILE: Web/Mvc/Z3InheritsQualified/InheritsQualifiedDemo.bas
' =================================================================================================
'       Z3 — INHERITS CON CLASSE BASE QUALIFICATA / DA ALTRO FILE (fix)
' =================================================================================================
' Il cliente IMPORTa il modulo MVC (HttpController.inc, nella stessa cartella) e definisce una
' classe che INHERITS la base QUALIFICATA MVC.HttpController. Stampa COUNT= 7.
' =================================================================================================

IMPORT MVC

PUBLIC CLASS UsersController INHERITS MVC.HttpController
    PUBLIC DIM Count AS LONG
END CLASS

SUB Main()
    DIM c AS UsersController = NEW UsersController()
    c.Count = 7
    PRINT "COUNT="; c.Count
END SUB
