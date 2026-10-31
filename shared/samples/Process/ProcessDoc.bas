' FILE: Process/ProcessDoc.bas
' E0 del piano automazione: PROCESS <file> PROMPT <istruzioni> — invia un documento
' all'agente con un prompt di elaborazione. Il runtime dispatcha per estensione
' (pdf/docx/xlsx/csv/md/html/rtf/epub/odt/pptx via ConversionLib; json/testo raw),
' estrae il testo e lo passa all'agente con le istruzioni; la risposta (riassunto/
' risultato) viene stampata.
'
' Verificato su IL e PCodeVM (completer fixture nei test).

PROCESS "../../Fixtures/process-report.txt" PROMPT "Estrai le revenue per divisione e riassumile in italiano."
