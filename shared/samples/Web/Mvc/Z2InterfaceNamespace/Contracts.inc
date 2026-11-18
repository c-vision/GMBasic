' FILE: Web/Mvc/Z2InterfaceNamespace/Contracts.inc
' =================================================================================================
'       Z2 — MODULO CON NAMESPACE: INTERFACE + CLASS IMPLEMENTS
' =================================================================================================
' Modulo .inc con NAMESPACE N (importabile con `IMPORT N`). Gap Z2 del piano MVC
' (, Fase Z): le PUBLIC CLASS
' (incluse ABSTRACT CLASS) si esportavano col namespace, le PUBLIC INTERFACE NO —
' `IMPORT N` + `DIM x AS N.IFoo` falliva ("tipo non trovato o ambiguo") mentre `N.Bar`
' funzionava. Il binder non entrava nello scope del NAMESPACE per le interfacce
' (InterfaceDefinition non è un Decl ed era senza case dedicato in BindModuleDeclarations).
' =================================================================================================

NAMESPACE N

PUBLIC INTERFACE IFoo
    FUNCTION GetX() AS LONG
END INTERFACE

PUBLIC CLASS Bar IMPLEMENTS IFoo
    PUBLIC FUNCTION GetX() AS LONG
        GetX = 42
    END FUNCTION
END CLASS
