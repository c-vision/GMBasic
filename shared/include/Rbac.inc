' FILE: include/Rbac.inc
' =================================================================================================
'       RBAC — autorizzazione per ruolo nei controller MVC / route (libreria di sistema, PURA)
' =================================================================================================
' Helper PURI (nessuna dipendenza da REQ/scope): ricevono il ruolo corrente, già estratto dalla
' sessione nel MAIN (dove REQ("Session") è un simbolo valido della richiesta) e decidono/scrivono.
'
' Uso in un'azione di un controller MVC (HttpController), all'inizio dell'azione:
'
'   DIM role$ = REQ("Session")("role")
'   IF NOT RBAC.Authorized("admin", role$) THEN
'       RBAC.Deny(SELF.Response, role$, "admin")
'       RETURN
'   END IF
'
' Nota: l'estrazione del ruolo dal canale esplicito REQ("Session") avviene al call-site (main),
' perché REQ è un simbolo dinamico della richiesta NON risolvibile dentro un modulo importato.
' Contratto: Authorized ritorna TRUE solo se role è valorizzato e (se required<>"") role==required;
' Deny scrive 401 (non autenticato) o 403 (ruolo insufficiente) su `res`. Import: `IMPORT RBAC`.

NAMESPACE RBAC

' TRUE se l'utente con ruolo `role` può accedere all'area che richiede `required`
' (required = "" → basta essere autenticati).
PUBLIC FUNCTION Authorized(required AS STRING, role AS STRING) AS BIT
    IF role = "" THEN RETURN FALSE
    IF required <> "" AND role <> required THEN RETURN FALSE
    RETURN TRUE
END FUNCTION

' Scrive la risposta d'errore appropriata: 401 se non autenticato (role=""), 403 se ruolo
' insufficiente. Non scrive nulla se autorizzato.
PUBLIC SUB Deny(res AS HTTPRESPONSE, role AS STRING, required AS STRING)
    IF role = "" THEN
        res.SetStatus(401)
    ELSE
        IF required <> "" AND role <> required THEN res.SetStatus(403)
    END IF
END SUB
