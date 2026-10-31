' FILE: Http/HttpFetchIntoCallback.bas
' =================================================================================================
'   FETCH ... INTO Callback — forma COMANDO asincrona
' =================================================================================================
'
' La forma SINCRONA e'   r = FETCH(url)   : restituisce subito un FETCHRESULT e blocca il task.
' La forma COMANDO e'    FETCH url[, opzioni] INTO Callback   (quella di questo esempio):
'
'   - il task CONTINUA a lavorare, la richiesta parte su un thread dedicato;
'   - a richiesta terminata la SUB di callback viene eseguita come TASK GPM con due parametri
'     `response$` (corpo della risposta) ed `error$` (testo dell'errore, VUOTO se tutto ok).
'
'   Attenzione 1 — la callback NON viene mai invocata dal thread di rete: e' un task GPM, quindi
'   rispetta la concorrenza del programma. Per questo qui si usa un WAITGROUP: senza qualcosa
'   che attenda, il programma puo' terminare prima che la callback arrivi e il lavoro si perde.
'
'   Attenzione 2 — `response$` ed `error$` sono STRING, non oggetti. Per lo stato HTTP (codice,
'   header, download su file) usa la forma sincrona, che restituisce un FETCHRESULT.
'
'   Attenzione 3 — la SUB di callback riceve UNA volta per richiesta. Se ti serve un flusso di
'   piu' richieste, ognuna ha la sua callback (oppure riusa la stessa SUB da richieste diverse,
'   guardingola con lo stato che ti serve).
'
' USO DELL'ESEMPIO
'   L'endpoint e' in chiaro in GMB_FETCH_URL, cosi' l'esempio e' adattabile a un servizio reale
'   senza toccare il resto. Di default punta a una porta locale non in ascolto: la richiesta
'   fallisce subito e l'output e' deterministico e verificabile SENZA rete.
'
'   Per provarlo con un servizio vero, cambia l'assegnazione in Main():
'     GMB_FETCH_URL = "https://www.uuidtools.com/api/generate/v4"
'   in quel caso `error$` resta vuoto e `response$` contiene il corpo della risposta.
'
' GAMBIE SUPPORTATE
'   IL, P-Code, VM/C, C99Native. Sul vecchio emitter C# la callback non veniva eseguita
'   (done restava 0): limite storico dell'emitter C#, ritirato con il backend C#.
' =================================================================================================

IMPORT HTTP
IMPORT CONCURRENCY

' Endpoint della richiesta: cambiabile da Main().
DIM GMB_FETCH_URL AS STRING

' Stato raccolto dalla callback. Sono STRING semplici perche' la callback gira come task GPM e
' comunica con il task che l'ha lanciata attraverso valori, non oggetti condivisi.
DIM gBody AS STRING
DIM gError AS STRING
DIM gDone AS INTEGER

' Callback della forma COMANDO: (response$, error$).
' Viene eseguita come task GPM quando la richiesta e' terminata.
SUB OnFetchComplete(resp$, err$)
  gBody = resp$
  gError = err$
  gDone = 1
END SUB

ASYNC SUB Starter(wg AS WAITGROUP)
  ' Lancia la richiesta e PROSEGUE: questo statement non blocca il task.
  FETCH GMB_FETCH_URL INTO OnFetchComplete()

  ' Il programma deve restare vivo finche' la callback non e' passata: questo e' il motivo per
  ' cui serve un WAITGROUP (senza, si uscirebbe dal programma e la callback non arriverebbe).
  DIM i AS INTEGER
  FOR i = 1 TO 400
    IF gDone = 1 THEN EXIT FOR
    AWAIT CONCURRENCY.SLEEPASYNC(25)
  NEXT i
  DONE wg
END SUB

SUB Main()
  GMB_FETCH_URL = "http://127.0.0.1:1/endpoint-non-in-ascolto"
  PRINT "endpoint:"; GMB_FETCH_URL

  DIM wg AS WAITGROUP
  GO SUB Starter(wg) IN wg
  WAIT wg

  ' Esito: i tre casi da distinguere sono "non eseguita", "fallita" (error$ non vuoto) e "ok".
  IF gDone = 0 THEN
    PRINT "callback: non eseguita entro il timeout"
  ELSEIF gError <> "" THEN
    PRINT "callback: eseguita, richiesta fallita"
    PRINT "  (gError contiene il testo dell'errore)"
  ELSE
    PRINT "callback: eseguita, richiesta ok"
    PRINT "  (response$ contiene il corpo della risposta)"
  END IF
END SUB