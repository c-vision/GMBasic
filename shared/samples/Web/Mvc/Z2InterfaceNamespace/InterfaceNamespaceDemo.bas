' FILE: Web/Mvc/Z2InterfaceNamespace/InterfaceNamespaceDemo.bas
' =================================================================================================
'       Z2 — INTERFACE ESPORTATE CON NAMESPACE (fix)
' =================================================================================================
' Cliente del modulo Contracts.inc (NAMESPACE N): la qualifica Ns.InterfaceType deve essere
' risolvibile nel linker, come già avveniva per le CLASS. Stampa 42.
' =================================================================================================

IMPORT N

SUB Main()
    DIM b AS N.IFoo = NEW N.Bar()
    PRINT b.GetX()
END SUB
