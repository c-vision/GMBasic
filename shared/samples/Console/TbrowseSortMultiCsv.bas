' TbrowseSortMultiCsv.bas — TBROWSE: ordinamento MULTI-COLONNA (sortkeys) + export CSV (§12.4).
' Il motore (gmb_browse) supporta N chiavi di ordinamento e l'export CSV della vista;
' qui si pilota dalla superficie via CONTROL SET/GETS. 4 backend.
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM sel AS INTEGER
  ' P/L dichiarata NUMERICA ("N"): ordina numericamente (non testualmente).
  TBROWSE 2, 2, 52, 10, {{"Simbolo", 12}, {"Settore", 12}, {"P/L", 10, "N"}}, {{"AAPL", "Tech", "120"}, {"MSFT", "Tech", "80"}, {"ENI", "Energy", "45"}, {"ENEL", "Utility", "45"}, {"NVDA", "Tech", "220"}} INTO sel NAME "book"

  ' Ordina per Settore (col 1) crescente; a parita', P/L (col 2) decrescente.
  CONTROL SET "book", "sortkeys", 0, "1:asc,2:desc"
  DIM sk AS STRING
  CONTROL GETS "book", "sortkeys" INTO sk
  PRINT "sortkeys="; sk

  ' Esporta la vista corrente in CSV.
  DIM csv AS STRING
  CONTROL GETS "book", "csv" INTO csv
  PRINT "CSV:"; CHR(10); csv

  PRINT "tbrowse-sortmulti-demo-ok"
END SUB
