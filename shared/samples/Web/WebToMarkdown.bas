' FILE: Web/WebToMarkdown.bas
' =================================================================================================
' TEST UNITARIO: WEB2MD — PAGINA WEB -> MARKDOWN
' =================================================================================================
' `WEB2MD <url> INTO <stringa>` scarica la pagina e la converte in Markdown nella variabile
' indicata (namespace CONVERT). È la via compatta per alimentare un LLM/RAG con contenuto web
' già pulito (senza HTML), oppure per archiviare una pagina in forma leggibile.
'
' COPERTURA:
' - WEB2MD : forma completa (URL + INTO); la conversione reale richiede un server raggiungibile,
'            quindi la chiamata è protetta da `IF 0 THEN` (convenzione già usata da
'            minitests/misc/surface_missing_symbols.bas per i simboli legati a risorse esterne):
'            il sample verifica la FIRMA e la sintassi del comando, che è quanto si può fare
'            senza rete.
'
' USO REALE: avviare un server locale (samples/Community/Network/) e togliere la guardia.
' =================================================================================================
IMPORT CONVERT

SUB Main()
    DIM url AS STRING
    DIM md AS STRING

    url = "http://localhost:8080/docs/index.html"

    IF 0 THEN
        WEB2MD url INTO md
        PRINT md
    END IF

    PRINT "WEB2MD — pagina web -> Markdown"
    PRINT "--------------------------------------------------"
    PRINT "URL di lavoro: "; url
    PRINT "Sintassi verificata: WEB2MD <url> INTO <stringa>"
    PRINT "Per la conversione reale servono: un server raggiungibile su "; url
    PRINT "e la rimozione della guardia IF 0 dal sample."
    PRINT "--------------------------------------------------"
    PRINT "Fine."
END SUB
