' EXPECT: call=FETCHED:https://example.com|senza-proc=[]
' MiniTest — AI.PLUGIN.CALL deve ESEGUIRE la procedura del PROGRAMMA (non un risultato
' dichiarativo). Il plugin (`shared/plugins/web-fetch/PLUGIN.md`) dichiara
' `procedures: [WebFetch]`; la procedura vive QUI, non nel plugin: il `.bas` dentro la
' cartella del plugin è riferimento e nessun backend lo carica.
' Convenzione argomenti (parità C# `PluginRegistry.Call` → `CallHandler` →
' `InvokeBasicProcedureGpm`): il primo argomento va nel PRIMO parametro; un plugin senza
' procedura nel programma (o inesistente) risponde "".
' 2026-10-31: chiusa la divergenza C99Native (rispondeva `PLUGIN nome(arg) OK` senza invocare)
' e allineata VM/C a `procedures[0]` di PLUGIN.md (prima derivava il nome dal nome del plugin:
' funzionava per `web-fetch`→`WebFetch`, non per `report`→`ReportGenerate`).
' LIMITE NOTO (non coperto qui): primo parametro NUMERICO — la VM/C impila l'argomento come
' stringa e il parametro numerico riceve 0; C#/C99 convertono ("42" → 42).
IMPORT AI WITH ALIAS

FUNCTION WebFetch(url AS STRING) AS STRING
  WebFetch = "FETCHED:" + url
END FUNCTION

SUB Main()
  PRINT "call="; AI.PLUGINCALL("web-fetch", "https://example.com")
  PRINT "senza-proc=["; AI.PLUGINCALL("pdf", "x"); "]"
END SUB
