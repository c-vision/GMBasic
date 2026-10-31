' FILE: Json/JsonFullTests.bas
' =================================================================================================
'       GUIDA TEORICA: TEST COMPLETO E UNIFICATO PER IL NAMESPACE JSON
' =================================================================================================
'
' Questo test è una suite di integrazione completa che verifica tutte le funzionalità del
' namespace `JSON`, dimostrando la corretta implementazione della sintassi
' (legacy prefissata e moderna qualificata) per i comandi e l'uso di tutte le funzioni.
'
' 1. SINTASSI DUALE PER I COMANDI
'    - Legacy: `JSON COMANDO arg1, arg2...` (senza parentesi)
'    - Moderna: `JSON.COMANDO arg1, arg2, ...` (senza parentesi)
'    Questo test esegue comandi come SAVE, WRITEVALUE, WRITESECTION e OPEN in entrambe le
'    forme per garantirne la parità funzionale.
'
' 2. COPERTURA COMPLETA DELLE FUNZIONI
'    Vengono testate tutte le funzioni del namespace, tra cui:
'    - `JSON.Parse`, `JSON.Stringify`: Conversione da/a stringa.
'    - `JSON.Load`: Caricamento da file.
'    - `JSON.ToJson`, `JSON.ToObject`: Conversione da/a tipi nativi (UDT, Collezioni).
'    - `JSON.ReadValue`, `JSON.ReadSection`: Lettura dal file di configurazione `app.settings.json`.
'    - `JSON.Extract`: Query su oggetti JSON complessi usando una sintassi a percorso.
'    - `JSON.Keys`: Introspezione delle proprietà di un oggetto.
'
' =================================================================================================

IMPORT JSON
IMPORT IO

' Tipo UDT per i test di conversione con ToJson e ToObject
TYPE ConfigData
    Host AS STRING * 50
    Timeout AS LONG
    Ports(1) AS INTEGER
END TYPE

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ JsonFullTests - Test Completo del Namespace JSON                       ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    ON ERROR GOTO GestoreErrori
    
    ' Output di categoria 4 (AGENTS §11): in `work/` accanto al sample (pattern `**/work/`
    ' gitignorato), mai nella cartella del sample.
    DIM WORK$ = "work"
    IF NOT IO.DIREXISTS(WORK$) THEN MKDIR WORK$
    DIM FILE_PATH$ = "work/test_suite.json"
    DIM SETTINGS_FILE$ = "app.settings.json"
    ' `JSON WRITEVALUE`/`WRITESECTION` scrivono SEMPRE nel settings della CWD (non accettano un
    ' percorso): un residuo di una gamba precedente o andata in errore cambia l'output. Stato
    ' azzerato prima dell'uso, cosi' il sample e' idempotente e parity4 non dipende dall'ordine
    ' delle gambe.
    IF IO.FILEEXISTS(SETTINGS_FILE$) THEN KILL SETTINGS_FILE$
    IF IO.FILEEXISTS(FILE_PATH$) THEN KILL FILE_PATH$
    DIM DATA$ = "{ ""user"": ""Mario"", ""score"": 1200, ""active"": true, ""roles"": [""admin"", ""editor""] }"
    
    ' --- 1. PARSE, Accesso a Campi, KEYS e EXTRACT ---
    PRINT "--- 1. Funzioni Base: PARSE, KEYS, EXTRACT ---"
    DIM DatiJson AS JSON = JSON.PARSE(DATA$)
    PRINT "  DatiJson.user = "; DatiJson.user; " (Atteso: Mario)"
    PRINT "  DatiJson.roles(1) = "; DatiJson.roles[1]; " (Atteso: editor)"
    ' Test della nuova funzione JSON.KEYS()
    DIM chiavi() AS STRING = JSON.KEYS(DatiJson)
    PRINT "  JSON.KEYS(): "; JOIN(chiavi, ", ")
    PRINT "  JSON.EXTRACT(DatiJson, ""roles[0]"") = "; JSON.EXTRACT(DatiJson, "roles[0]"); " (Atteso: admin)"
    PRINT

    ' --- 2. Modifica e STRINGIFY ---
    PRINT "--- 2. Modifica e STRINGIFY ---"
    DatiJson.score = 1500
    PRINT "  Nuovo punteggio: "; DatiJson.score
    PRINT "  Stringify: "; JSON.STRINGIFY(DatiJson)
    PRINT

    ' --- 3. SAVE e LOAD (Sintassi legacy Qualificata) ---
    PRINT "--- 3. SAVE e LOAD (Sintassi Legacy) ---"
    JSON SAVE DatiJson, FILE_PATH$
    PRINT "  Oggetto salvato su '"; FILE_PATH$; "' con JSON SAVE."
    DIM LoadedJson AS JSON = JSON.LOAD(FILE_PATH$)
    PRINT "  Oggetto ricaricato. Utente: "; LoadedJson.user; " (Atteso: Mario)"
    PRINT
    
    ' --- 3B. SAVE (Sintassi Legacy) ---
    PRINT "--- 3B. SAVE (Sintassi Legacy) ---"
    KILL FILE_PATH$ ' Pulisce il file precedente
    JSON SAVE DatiJson, FILE_PATH$
    PRINT "  Oggetto salvato su '"; FILE_PATH$; "' con JSON SAVE."
    IF IO.FileExists(FILE_PATH$) THEN PRINT "  Verifica: File creato correttamente."
    PRINT

    ' --- 4. READ/WRITE su app.settings.json (Sintassi Legacy) ---
    PRINT "--- 4. READ/WRITE su app.settings.json (Sintassi Legacy) ---"
    JSON WRITEVALUE "ApiKey", "ABC-123-XYZ"
    PRINT "  Scritto 'ApiKey' con JSON WRITEVALUE."
    DIM ApiKey$ = JSON.ReadValue("ApiKey")
    PRINT "  Letto 'ApiKey': "; ApiKey$; " (Atteso: ABC-123-XYZ)"
    
    DIM Cfg AS ConfigData
    Cfg.Host = "api.server.com"
    Cfg.Timeout = 5000
    REDIM Cfg.Ports(1) : Cfg.Ports = {80, 443}
    JSON WRITESECTION Cfg, "ServerConfig"
    PRINT "  Scritta sezione 'ServerConfig' da UDT con JSON WRITESECTION."
    
    DIM ReadCfg AS JSON = JSON.ReadSection("ServerConfig")
    PRINT "  Letto 'ServerConfig.Timeout': "; ReadCfg.Timeout; " (Atteso: 5000)"
    PRINT
    
    ' --- 4B. WRITE su app.settings.json (Sintassi Legacy) ---
    PRINT "--- 4B. WRITE su app.settings.json (Sintassi Legacy) ---"
    JSON WRITEVALUE "LegacyKey", "ValoreLegacy"
    PRINT "  Scritto 'LegacyKey' con JSON WRITEVALUE."
    IF JSON.ReadValue("LegacyKey") = "ValoreLegacy" THEN PRINT "  Verifica lettura: OK."
    JSON WRITESECTION Cfg, "LegacyServerConfig"
    PRINT "  Scritta sezione 'LegacyServerConfig' con JSON WRITESECTION."
    IF JSON.ReadSection("LegacyServerConfig").Host = "api.server.com" THEN PRINT "  Verifica lettura sezione: OK."
    PRINT

    ' --- 5. Conversione Oggetti <-> JSON ---
    PRINT "--- 5. Conversione Oggetti <-> JSON (ToJson, ToObject) ---"
    DIM MyUdt AS ConfigData : MyUdt.Host = "test.host" : MyUdt.Timeout = 1234 : REDIM MyUdt.Ports(0) : MyUdt.Ports = {8080}
    DIM UdtAsJson AS JSON = JSON.ToJson(MyUdt)
    PRINT "  UDT convertito in JSON: "; UdtAsJson.Host
    
    DIM NewUdt AS ConfigData = JSON.ToObject(ConfigData, UdtAsJson)
    PRINT "  JSON riconvertito in UDT. Timeout: "; NewUdt.Timeout; " (Atteso: 1234)"
    PRINT "  Porta[0] nell'UDT riconvertito: "; NewUdt.Ports(0)
    PRINT

    ' --- 6. I/O su file con JSON.OPEN (Nuova Sintassi) ---
    PRINT "--- 6. Test I/O su file con JSON OPEN ---"
    DIM FILE_JSON_2$ = "work/data2.json"
    DIM h AS LONG
    ' Scrittura
    JSON OPEN FILE_JSON_2$ FOR "WRITE" AS #h
    PRINT #h, DatiJson
    CLOSE #h
    PRINT "  File 'data2.json' scritto con JSON OPEN."
    ' Lettura
    DIM DatiLetti2 AS JSON
    JSON OPEN FILE_JSON_2$ FOR READ AS #h
    INPUT #h, DatiLetti2
    CLOSE #h
    PRINT "  File 'data2.json' letto con JSON OPEN. Utente: "; DatiLetti2.user
    PRINT
    
    GOTO Pulizia

GestoreErrori:
    PRINT "ERRORE INASPETTATO: #" ; ERR.Number; " - "; ERR.Description
    
Pulizia:
    PRINT
    PRINT "--- Pulizia dei file di test ---"
    ON ERROR RESUME NEXT
    KILL FILE_PATH$
    KILL FILE_JSON_2$
    KILL SETTINGS_FILE$
    ON ERROR GOTO 0
    PRINT "  Pulizia completata."
    PRINT
    PRINT "--- TEST JSON COMPLETATO ---"
END SUB