' FILE: Web/Mvc/Z4SetStatusPropagates/SetStatusDemo.bas
' =================================================================================================
'       Z4 — res.SetStatus()/SetHeader() DA UN SUB DI CLASSE PROPAGANO (fix)
' =================================================================================================
' Gap Z4 del piano MVC (, Fase Z):
' un metodo di classe che chiamava res.SetStatus(404) NON modificava la risposta effettiva
' (status restava 200) mentre l'assegnazione proprietà res.StatusCode = 404 propagava.
' Causa radice: lo schema del tipo HttpResponseType dichiarava come METODI solo
' Send/SendJSON/SendBytes/SendFile/SetCookie/Render/Redirect — SetStatus e SetHeader
' mancavano, quindi il linker li risolveva come CAMPI (res.SetStatus(404) -> OP_LOAD_FIELD
' 'SetStatus' + OP_LOAD_ELEMENT, un no-op) invece che come MethodCallExpr -> OP_CALL_MEMBER.
' Fix: aggiunti SetStatus/Status/SetHeader allo schema METODI di HttpResponseType
' (common/StdLib/StdLib.Http.cs) — coerenti coi case già presenti in OP_CALL_MEMBER.
'
' Verifica: HTTP GET /setstatus e /property devono entrambi rispondere 404.
' =================================================================================================

IMPORT HTTP

PUBLIC CLASS MyController
    PUBLIC SUB ViaSetStatus(res AS HTTPRESPONSE)
        res.SetStatus(404)
        res.Send("VIA-SETSTATUS")
    END SUB
    PUBLIC SUB ViaSetHeader(res AS HTTPRESPONSE)
        res.SetHeader("X-Custom", "yes")
        res.StatusCode = 404
        res.Send("VIA-SETHEADER")
    END SUB
END CLASS

DIM ctl AS MyController = NEW MyController

HTTP ROUTE "GET", "/setstatus", CtrlSetStatus()
HTTP ROUTE "GET", "/setheader", CtrlSetHeader()

SUB CtrlSetStatus(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    ctl.ViaSetStatus(res)
END SUB
SUB CtrlSetHeader(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    ctl.ViaSetHeader(res)
END SUB

DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 18082
HTTP CONFIG cfg
HTTP SERVER cfg.Port

DIM wg AS WAITGROUP
GO SUB KeepAlive(wg) IN wg
WAIT wg

SUB KeepAlive(wg AS WAITGROUP)
    SLEEP 3600000
END SUB
