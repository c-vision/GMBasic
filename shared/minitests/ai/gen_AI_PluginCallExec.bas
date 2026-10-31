' EXPECT: call=FETCHED:https://example.com|senza-proc=[]|num-arg=FETCHED:42|primo-int=NUM: 42:
' MiniTest — AI.PLUGIN.CALL deve ESEGUIRE la procedura del PROGRAMMA (non un risultato
' dichiarativo). Il plugin (`shared/plugins/web-fetch/PLUGIN.md`) dichiara
' `procedures: [WebFetch]`; la procedura vive QUI, non nel plugin: il `.bas` dentro la
' cartella del plugin è riferimento e nessun backend lo carica.
' Convenzione argomenti (parità C# `PluginRegistry.Call` → `CallHandler` →
' `InvokeBasicProcedureGpm`): il primo argomento va nel PRIMO parametro; un plugin senza
' procedura nel programma (o inesistente) risponde "".
' 2026-10-31: chiusa la divergenza C99Native (rispondeva `PLUGIN nome(arg) OK` senza invocare),
' allineata VM/C a `procedures[0]` di PLUGIN.md e chiuso il caso del PRIMO PARAMETRO NUMERICO
' (VM/C impilava sempre una stringa → il parametro numerico riceveva 0).
IMPORT AI WITH ALIAS

FUNCTION WebFetch(url AS STRING) AS STRING
  WebFetch = "FETCHED:" + url
END FUNCTION

' prima procedura del plugin "report": primo parametro NUMERICO.
FUNCTION ReportGenerate(rows AS INTEGER, title AS STRING) AS STRING
  ReportGenerate = "NUM:" + STR$(rows) + ":" + title
END FUNCTION

SUB Main()
  PRINT "call="; AI.PLUGINCALL("web-fetch", "https://example.com")
  PRINT "senza-proc=["; AI.PLUGINCALL("pdf", "x"); "]"
  ' argomento numerico verso un parametro STRINGA (numero → stringa)
  PRINT "num-arg="; AI.PLUGINCALL("web-fetch", "42")
  ' argomento numerico verso un parametro NUMERICO (stringa → numero), secondo parametro default
  PRINT "primo-int="; AI.PLUGINCALL("report", "42")
END SUB
