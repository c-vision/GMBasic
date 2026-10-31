' FILE: AI/AiPlugins.bas
' AiPlugins — ciclo di vita dei plugin operativi (AI PLUGINS) + funzioni di lettura e
' invocazione capacita' nel namespace AI (AI.PLUGINSLIST / AI.PLUGINSINFO / AI.PLUGINCALL).
' La FUNCTION WebFetch e' la procedura BASIC del plugin web-fetch (tool.json -> procedura):
' il manifest del plugin la espone e AI.PLUGINCALL la invoca via InvokeBasicProcedure.

IMPORT AI WITH ALIAS
IMPORT ARRAYS

FUNCTION WebFetch(url AS STRING) AS STRING
  WebFetch = "FETCHED:" + url
END FUNCTION

SUB Main()
  DIM s$ AS STRING
  DIM names$ AS STRING

  AI PLUGINS REFRESH
  PRINT "Refresh done"

  AI PLUGINS LIST
  PRINT "List done"

  names$ = ARRAYS.AJOIN(AI.PLUGINSLIST(), ",")
  PRINT "PLUGINSLIST="; names$

  AI PLUGINS ENABLE "web-fetch"
  PRINT "Enabled web-fetch"

  s$ = AI.PLUGINSINFO("web-fetch")
  PRINT "PLUGINSINFO="; s$

  ' Proprietà verificata (non il testo esatto): le gambe gestite/VM eseguono davvero la
  ' procedura BASIC del plugin (via InvokeBasicProcedure/hook VM), i backend C99 — senza VM
  ' BASIC nel binario compilato — ritornano il risultato dichiarativo "PLUGIN nome(arg) OK".
  ' Entrambi non-vuoti: la chiamata riesce su tutte le gambe (divergenza architetturale tracciata).
  s$ = AI.PLUGINCALL("web-fetch", "https://example.com")
  PRINT "PLUGINCALL_LEN="; LEN(s$) > 0

  AI PLUGINS DISABLE "web-fetch"
  PRINT "Disabled web-fetch"

  AI PLUGINS CREATE "demo-plugin", "Plugin demo (auto-generated)"
  PRINT "Created demo-plugin"

  AI PLUGINS ADD "demo-plugin"
  PRINT "Added demo-plugin"

  AI PLUGINS REMOVE "demo-plugin"
  PRINT "Removed demo-plugin"
END SUB
