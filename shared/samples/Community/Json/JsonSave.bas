' FILE: Json/JsonSave.bas

' ======================================================================================================================
' GUIDA TEORICA: IL NAMESPACE JSON
' ======================================================================================================================
' Il namespace JSON fornisce funzionalità complete per la serializzazione, deserializzazione e manipolazione di dati
' in formato JSON (JavaScript Object Notation), oltre alla gestione di file di configurazione e I/O strutturato.
'
' 1. SERIALIZZAZIONE E DESERIALIZZAZIONE
'    - JSON.PARSE(jsonString$): Converte una stringa JSON in un oggetto dinamico di tipo JSON. L'oggetto risultante
'      permette l'accesso ai campi tramite la sintassi a punto (obj.campo) o indice (obj[0]).
'    - JSON.STRINGIFY(jsonObj): Converte un oggetto JSON (o un UDT/Collezione) in una stringa JSON formattata.
'    - JSON.TOJSON(obj): Converte un tipo nativo (UDT, List, Dict) in un oggetto dinamico JSON.
'    - JSON.TOOBJECT(Type, jsonObj): Converte un oggetto JSON in un tipo nativo fortemente tipizzato.
'
' 2. PERSISTENZA SU FILE
'    - JSON.SAVE obj, path$: Serializza un oggetto e lo salva direttamente su un file di testo.
'    - JSON.LOAD(path$): Legge un file di testo e lo deserializza in un oggetto JSON.
'    - JSON.OPEN path$ FOR mode$ AS #handle: Apre un file JSON per I/O sequenziale strutturato (modalità avanzata).
'
' 3. CONFIGURAZIONE APPLICAZIONE (app.settings.json)
'    GM-BASIC integra un sistema di gestione della configurazione basato su JSON.
'    - JSON.WRITEVALUE key$, val: Scrive un valore nel file di impostazioni globale.
'    - JSON.READVALUE(key$): Legge un valore dal file di impostazioni.
'    - JSON.WRITESECTION udt, section$: Serializza un intero UDT in una sezione specifica delle impostazioni.
'    - JSON.READSECTION(section$): Legge una sezione e restituisce un oggetto JSON (convertibile in UDT).
'
' 4. MANIPOLAZIONE E INTROSPEZIONE
'    - JSON.EXTRACT(obj, path$): Estrae un valore annidato usando una sintassi di percorso (es. "users[0].name").
'    - JSON.KEYS(obj): Restituisce un array di stringhe contenente le chiavi di un oggetto JSON.
' ======================================================================================================================

IMPORT JSON
IMPORT IO

TYPE ConfigData
    Host AS STRING * 50
    Timeout AS LONG
END TYPE

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ JsonSave - Test Completo del Namespace JSON                            ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    ON ERROR GOTO GestoreErrori
    
    ' Output della categoria 4 (AGENTS §11): in `work/` accanto al sample, mai nella cartella
    ' del sample (il pattern `**/work/` e' gitignorato).
    DIM WORK$ = "work"
    IF NOT IO.DIREXISTS(WORK$) THEN MKDIR WORK$
    DIM FILE_PATH$ = "work/test_suite.json"
    DIM SETTINGS_FILE$ = "app.settings.json"
    ' `JSON WRITEVALUE`/`WRITESECTION` scrivono SEMPRE nel file di settings della CWD (non
    ' accettano un percorso). Un residuo lasciato da una gamba precedente o da una gamba andata
    ' in errore cambia l'output: si azzera lo stato condiviso prima dell'uso, cosi' il sample e'
    ' idempotente e parity4 non dipende dall'ordine delle gambe.
    IF IO.FILEEXISTS(SETTINGS_FILE$) THEN KILL SETTINGS_FILE$
    IF IO.FILEEXISTS(FILE_PATH$) THEN KILL FILE_PATH$
    DIM DATA$ = "{ ""user"": ""Mario"", ""score"": 1200, ""active"": true, ""roles"": [""admin"", ""editor""] }"
    
    ' --- 1. PARSE, Accesso a Campi e EXTRACT ---
    PRINT "--- 1. PARSE, Accesso a Campi, EXTRACT e KEYS ---"
    ' JSON.PARSE crea un oggetto dinamico dalla stringa
    DIM DatiJson AS JSON = JSON.PARSE(DATA$)
    
    ' Accesso diretto ai campi dinamici
    PRINT "  DatiJson.user = "; DatiJson.user; " (Atteso: Mario)"
    ' Accesso agli elementi di un array JSON tramite indice
    PRINT "  DatiJson.roles(1) = "; DatiJson.roles[1]; " (Atteso: editor)"
    ' JSON.EXTRACT permette di navigare la struttura con una stringa di percorso
    PRINT "  JSON.EXTRACT(DatiJson, ""roles[0]"") = "; JSON.EXTRACT(DatiJson, "roles[0]"); " (Atteso: admin)"

    ' VERIFICA DI COMPLETEZZA: Test funzione JSON.KEYS
    PRINT "  Test JSON.KEYS:"
    DIM keys() AS STRING
    keys = JSON.KEYS(DatiJson)
    DIM k
    FOR k = 0 TO UBOUND(keys)
        PRINT "    Key["; k; "] = "; keys(k)
    NEXT
    PRINT

    ' --- 2. Modifica e STRINGIFY ---
    PRINT "--- 2. Modifica e STRINGIFY ---"
    ' Modifica di un campo dinamico
    DatiJson.score = 1500
    PRINT "  Nuovo punteggio: "; DatiJson.score
    ' Serializzazione inversa in stringa
    PRINT "  Stringify: "; JSON.STRINGIFY(DatiJson)
    PRINT

    ' --- 3. SAVE e LOAD ---
    PRINT "--- 3. SAVE e LOAD ---"
    ' Salvataggio diretto su file
    JSON SAVE DatiJson, FILE_PATH$
    PRINT "  Oggetto salvato su '"; FILE_PATH$; "'."
    
    ' Caricamento diretto da file
    DIM LoadedJson AS JSON = JSON.LOAD(FILE_PATH$)
    PRINT "  Oggetto ricaricato. Utente: "; LoadedJson.user; " (Atteso: Mario)"
    PRINT
    
    ' --- 4. READ/WRITE su app.settings.json ---
    PRINT "--- 4. READ/WRITE su app.settings.json ---"
    ' Scrittura di un singolo valore nel file di configurazione di sistema
    JSON WRITEVALUE "ApiKey", "ABC-123-XYZ"
    PRINT "  Scritto 'ApiKey'."
    ' Lettura di un singolo valore
    DIM ApiKey$ = JSON.ReadValue("ApiKey")
    PRINT "  Letto 'ApiKey': "; ApiKey$; " (Atteso: ABC-123-XYZ)"
    
    DIM Cfg AS ConfigData
    Cfg.Host = "api.server.com"
    Cfg.Timeout = 5000
    ' Serializzazione di un UDT in una sezione specifica della configurazione
    JSON WRITESECTION Cfg, "ServerConfig"
    PRINT "  Scritta sezione 'ServerConfig' da UDT."
    
    ' Lettura di un'intera sezione come oggetto JSON
    DIM ReadCfg AS JSON = JSON.ReadSection("ServerConfig")
    PRINT "  Letto 'ServerConfig.Timeout': "; ReadCfg.Timeout; " (Atteso: 5000)"
    PRINT
    
    ' --- 5. Conversione Oggetti <-> JSON ---
    PRINT "--- 5. Conversione Oggetti <-> JSON ---"
    DIM MyUdt AS ConfigData : MyUdt.Host = "test.host" : MyUdt.Timeout = 1234
    ' Conversione da UDT a JSON dinamico
    DIM UdtAsJson AS JSON = JSON.ToJson(MyUdt)
    PRINT "  UDT convertito in JSON: "; UdtAsJson.Host
    
    ' VERIFICA DI COMPLETEZZA: Abilitazione test ToObject per UDT
    ' Converte un oggetto JSON dinamico in un UDT fortemente tipizzato
    DIM NewUdt AS ConfigData = JSON.ToObject(ConfigData, UdtAsJson)
    PRINT "  JSON riconvertito in UDT. Timeout: "; NewUdt.Timeout; " (Atteso: 1234)"

    DIM MyList AS LIST OF INTEGER = [10, 20, 30]
    ' Conversione da List a JSON dinamico (array)
    DIM ListAsJson AS JSON = JSON.ToJson(MyList)
    PRINT "  Lista convertita in JSON: "; JSON.Stringify(ListAsJson)
    
    DIM JsonAsList AS LIST OF INTEGER
    ' Conversione da JSON array a List
    JsonAsList = JSON.ToObject(LIST OF INTEGER, ListAsJson)
    PRINT "  JSON riconvertito in Lista. Count: "; JsonAsList.Count ' Atteso 3
    PRINT

    ' --- 6. VERIFICA DI COMPLETEZZA: TEST I/O JSON STREAMING (OPEN) ---
    PRINT "--- 6. Test JSON OPEN (Streaming I/O) ---"
    DIM hJson
    ' Apre un file JSON in modalità Input per lettura sequenziale/strutturata
    ' Questo comando prepara il file per essere letto come un oggetto JSON
    JSON OPEN FILE_PATH$ FOR "INPUT" AS #hJson
    
    ' Nota: Al momento JSON OPEN prepara l'handle. La lettura effettiva avverrebbe tramite
    ' INPUT #hJson o deserializzazione stream, funzionalità avanzata da implementare/verificare nell'Executor.
    ' Per ora verifichiamo che il comando di apertura e chiusura non dia errori.
    PRINT "  File JSON aperto correttamente con handle: "; hJson
    CLOSE #hJson
    PRINT "  File JSON chiuso."

    GOTO Pulizia

GestoreErrori:
    PRINT "ERRORE INASPETTATO: #" ; ERR.Number; " - "; ERR.Description
    
Pulizia:
    PRINT
    PRINT "--- Pulizia dei file di test ---"
    ON ERROR RESUME NEXT
    KILL FILE_PATH$
    KILL SETTINGS_FILE$
    ON ERROR GOTO 0
    PRINT "  Pulizia completata."
    PRINT
    PRINT "--- TEST JSON COMPLETATO ---"
END SUB