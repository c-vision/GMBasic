' FILE: Security/ValidationFluentDemo.bas
' =================================================================================================
'       VALIDATION — libreria include/Validation.inc (regole su STRINGS.IS*)
' =================================================================================================
' Accumula errori in un JSON `errors` e li verifica (pattern per form/MVC). Richiede
' IMPORT STRINGS e IMPORT JSON nel programma.

IMPORT VALIDATION
IMPORT STRINGS
IMPORT JSON

PRINT "=== Validation fluent ==="

DIM errors AS JSON = NEW JSON
VALIDATION.Required(errors, "nome", "", "Nome richiesto")
VALIDATION.Email(errors, "mail", "not-an-email", "Email non valida")
VALIDATION.Url(errors, "site", "ftp://x", "URL non valida")
VALIDATION.Length(errors, "pw", "abc", 8, 64, "Min 8 caratteri")
VALIDATION.CreditCard(errors, "cc", "1234567890123456", "Carta non valida")

IF NOT VALIDATION.IsValid(errors) THEN
    PRINT "INVALID OK"
ELSE
    PRINT "INVALID FAIL"
END IF

DIM errors2 AS JSON = NEW JSON
VALIDATION.Email(errors2, "mail", "a@b.co", "")
VALIDATION.Required(errors2, "nome", "Alice", "")
IF VALIDATION.IsValid(errors2) THEN
    PRINT "VALID OK"
ELSE
    PRINT "VALID FAIL"
END IF

PRINT "ValidationFluent done"
