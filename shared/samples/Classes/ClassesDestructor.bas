' FILE: samples/Classes/ClassesDestructor.bas
' =================================================================================================
'   GUIDA TEORICA: Distruttori (SUB DESTRUCTOR)
' =================================================================================================
' Il distruttore è un metodo speciale eseguito automaticamente quando un'istanza viene distrutta.
' - Si scrive `PUBLIC SUB DESTRUCTOR ... END SUB`, SENZA parentesi (a differenza di SUB NEW).
' - Deve essere PUBLIC; una classe/TYPE può avere un solo distruttore.
' - NON riceve parametri e non ritorna valori.
' - Serve a rilasciare risorse: file aperti, handle, buffer, connessioni.
'
' NOTA STORICA: la keyword era originariamente `DELETE`; è stata rinominata in `DESTRUCTOR`
' perché `DELETE` è anche il verbo REST e il comando DB. Oggi `SUB Delete(...)` è un metodo
' normale; il distruttore è `SUB DESTRUCTOR`.
'
' COMPORTAMENTO REALE (verificato sul backend PCodeVM, 2026-09-01):
' - Le variabili GLOBALI di tipo classe/TYPE vengono distrutte (e quindi il loro DESTRUCTOR
'   chiamato) alla FINE del programma, in ORDINE INVERSO di creazione.
' - Le istanze LOCALI (dentro SUB/FUNCTION) NON vengono finalizzate automaticamente dal runtime:
'   se una risorsa locale deve essere rilasciata, il programma la rilascia esplicitamente
'   (chiamando un metodo di chiusura) — NON affidarsi al distruttore per le variabili locali.
' - La Riassegnazione di una variabile globale (g = NEW X()) NON finalizza la vecchia istanza.
'
' Questo sample dimostra:
'   1. sintassi del distruttore e ciclo di vita delle risorse GLOBALI (apertura/chiusura);
'   2. un contatore STATICO di risorse "vive" (accesso con NomeClasse.Campo, NON SELF.Campo);
'   3. l'ordine di distruzione (inverso della creazione) a fine programma;
'   4. che le istanze locali NON vengono finalizzate (documentato nel commento).
' =================================================================================================

' --- Risorsa con contatore statico delle istanze "vive" ---
PUBLIC CLASS Risorsa
    PUBLIC DIM Nome AS STRING
    PUBLIC STATIC DIM Aperte AS LONG = 0

    PUBLIC SUB NEW(nome AS STRING)
        SELF.Nome = nome
        Risorsa.Aperte = Risorsa.Aperte + 1      ' accesso allo STATIC: NomeClasse.Campo
        PRINT "  [aperta risorsa] "; SELF.Nome; "   (risorse aperte: "; Risorsa.Aperte; ")"
    END SUB

    PUBLIC SUB DESTRUCTOR
        Risorsa.Aperte = Risorsa.Aperte - 1
        PRINT "  [chiusa risorsa] "; SELF.Nome; "   (risorse ancora aperte: "; Risorsa.Aperte; ")"
    END SUB
END CLASS

' --- Risorsa "pesante" con handle simulato: rilascio nel distruttore ---
PUBLIC CLASS Connessione
    PUBLIC DIM Endpoint AS STRING
    PRIVATE DIM _handle AS LONG

    PUBLIC SUB NEW(endpoint AS STRING, handle AS LONG)
        SELF.Endpoint = endpoint
        SELF._handle = handle
        PRINT "  [connesso] "; SELF.Endpoint; " handle="; SELF._handle
    END SUB

    PUBLIC FUNCTION Ping() AS STRING
        FUNCTION = "pong(" + SELF.Endpoint + ")"
    END FUNCTION

    PUBLIC SUB Chiudi()
        PRINT "  [chiusura esplicita] "; SELF.Endpoint
        SELF._handle = 0
    END SUB

    PUBLIC SUB DESTRUCTOR
        IF SELF._handle <> 0 THEN
            PRINT "  [disconnesso (automatico)] "; SELF.Endpoint; " handle="; SELF._handle
            SELF._handle = 0
        END IF
    END SUB
END CLASS

' --- UDT (TYPE) con distruttore (nei TYPE niente PUBLIC, i campi senza DIM) ---
TYPE BufferT
    Id AS LONG
    Size AS LONG
    SUB NEW(id AS LONG, size AS LONG)
        SELF.Id = id
        SELF.Size = size
    END SUB
    SUB DESTRUCTOR
        PRINT "  [buffer liberato] id="; SELF.Id; " size="; SELF.Size
    END SUB
END TYPE

' --- Scenario: istanza LOCALE (documentazione del limite) ---
SUB ScenarioLocale()
    PRINT ">> Scenario 1: risorsa locale dentro una SUB"
    DIM c AS Connessione = NEW Connessione("tcp://server:8080", 42)
    PRINT "   ping: "; c.Ping()
    PRINT "   NB: a fine SUB l'istanza locale NON viene finalizzata automaticamente;"
    PRINT "   per rilasciarla davvero va chiamato un metodo esplicito:"
    c.Chiudi()
END SUB

' --- Risorse GLOBALI: verranno distrutte a fine programma (ordine inverso) ---
DIM gLog AS Risorsa = NEW Risorsa("config.ini")
DIM gDb AS Connessione = NEW Connessione("tcp://db:5432", 7)
DIM gCache AS BufferT = NEW BufferT(99, 8192)

SUB Main()
    PRINT "== Classi — Distruttori (SUB DESTRUCTOR) =="
    PRINT

    ScenarioLocale()
    PRINT

    PRINT ">> Scenario 2: buffer UDT globale (distrutto a fine programma)"
    PRINT "   cache id="; gCache.Id; " size="; gCache.Size
    PRINT

    PRINT ">> Scenario 3: riassegnazione di una variabile globale"
    PRINT "   NB: la riassegnazione NON finalizza la vecchia istanza (config.ini resta"
    PRINT "   non finalizzata): il distruttore gira solo per le globali ancora vive a fine"
    PRINT "   programma e per le riassegnazioni esplicite si gestisce il rilascio a mano."
    gLog = NEW Risorsa("config_v2.ini")
    PRINT

    PRINT ">> Fine programma: distruzione automatica delle variabili globali ancora vive"
    PRINT "   (gLog(v2), gCache, gDb — config.ini originale non è mai stato finalizzato)"
    PRINT
END SUB
