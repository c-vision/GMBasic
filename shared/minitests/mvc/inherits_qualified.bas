' EXPECT: COUNT= 7  DUMMY= 3
' §4.1 — INHERITS con base QUALIFICATA (MVC.HttpController in un modulo .inc) + campo EREDITATO.
' Copre: parser (nome base multi-segmento), struct FLAT del derivato (campi ereditati),
' lookup dei campi ereditati. NB: niente `END NAMESPACE` (non esiste nel linguaggio).
IMPORT MVC

PUBLIC CLASS UsersController INHERITS MVC.HttpController
    PUBLIC DIM Count AS LONG
END CLASS

SUB Main()
    DIM c AS UsersController = NEW UsersController()
    c.Count = 7
    c.Dummy = 3
    PRINT "COUNT="; c.Count; " DUMMY="; c.Dummy
END SUB
