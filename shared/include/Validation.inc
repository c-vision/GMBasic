' FILE: include/Validation.inc
' =================================================================================================
'       VALIDATION — validazione esplicita per form/input (libreria di sistema)
' =================================================================================================
' Libreria BASIC di sistema (import: `IMPORT VALIDATION`) costruita sui builtin
' `STRINGS.ISREQUIRED/ISEMAIL/ISURL/ISCREDITCARD/ISLENGTH`. Stile esplicito: si accumulano
' errori in un JSON `errors` passato per riferimento; invio 400 con { "errors": ... } a scelta.
' Richiede nel programma: IMPORT STRINGS e IMPORT JSON.

NAMESPACE VALIDATION

' --- Regole: aggiungono l'errore (default/messaggio custom) solo se il check fallisce ---
PUBLIC SUB Required(errors AS JSON, field AS STRING, value AS STRING, message AS STRING)
    IF STRINGS.ISREQUIRED(value) = FALSE THEN errors[field] = message
END SUB

PUBLIC SUB Email(errors AS JSON, field AS STRING, value AS STRING, message AS STRING)
    IF STRINGS.ISEMAIL(value) = FALSE THEN errors[field] = message
END SUB

PUBLIC SUB Url(errors AS JSON, field AS STRING, value AS STRING, message AS STRING)
    IF STRINGS.ISURL(value) = FALSE THEN errors[field] = message
END SUB

PUBLIC SUB CreditCard(errors AS JSON, field AS STRING, value AS STRING, message AS STRING)
    IF STRINGS.ISCREDITCARD(value) = FALSE THEN errors[field] = message
END SUB

PUBLIC SUB Length(errors AS JSON, field AS STRING, value AS STRING, minLength AS LONG, maxLength AS LONG, message AS STRING)
    IF STRINGS.ISLENGTH(value, minLength, maxLength) = FALSE THEN errors[field] = message
END SUB

' --- Esito: true se non ci sono errori (errors = "{}") ---
PUBLIC FUNCTION IsValid(errors AS JSON) AS BIT
    RETURN JSON.STRINGIFY(errors) = "{}"
END FUNCTION
