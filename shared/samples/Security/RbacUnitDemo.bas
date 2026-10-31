' FILE: Security/RbacUnitDemo.bas
' =================================================================================================
'       SECURITY — RBAC helper (include/Rbac.inc): unit della logica pura (Authorized)
' =================================================================================================
' Verifica RBAC.Authorized/Deny senza rete (logica pura su stringhe). L'estrazione del ruolo da
' REQ("Session") vive al call-site (main/controller) ed è coperta dai demo REPL
' RbacControllerDemo.bas / RbacRouteDemo.bas.

IMPORT RBAC

PRINT "=== RBAC unit ==="

IF RBAC.Authorized("admin", "admin") THEN
    PRINT "ADMIN OK"
ELSE
    PRINT "ADMIN FAIL"
END IF

IF RBAC.Authorized("admin", "user") THEN
    PRINT "ROLE DENY FAIL"
ELSE
    PRINT "ROLE DENY OK"
END IF

IF RBAC.Authorized("admin", "") THEN
    PRINT "NOTAUTH FAIL"
ELSE
    PRINT "NOTAUTH OK"
END IF

' required = "" → basta essere autenticati
IF RBAC.Authorized("", "user") THEN
    PRINT "AUTH ONLY OK"
ELSE
    PRINT "AUTH ONLY FAIL"
END IF

PRINT "RbacUnitDemo done"
