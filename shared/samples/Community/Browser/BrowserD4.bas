' BrowserD4.bas — D4: download PDF + estrazione testo (CONVERT.ToText / gmb_doc_pdf.c)
' + ingestion → RAG (vector DB) con ricerca end-to-end.
' Il PDF è servito come base64 dal server locale (http://127.0.0.1:8104/d4.pdf.b64).
' Il sample include SOLO BrowserDownloadPdf (il corpo plugin completo interagisce col
' codegen IL flaky — in indagine condivisa, vedi guida Browser-Automation).

IMPORT AI WITH ALIAS
IMPORT SQL WITH ALIAS
IMPORT CONVERT
IMPORT CRYPTOGRAPHY

FUNCTION BrowserDownloadPdf(port AS INTEGER, url AS STRING, destDir AS STRING, fileName AS STRING) AS STRING
  DIM r AS FETCHRESULT = FETCH(url)
  DIM b64 AS STRING
  DIM path AS STRING
  DIM text AS STRING
  b64 = r.Body
  IF LEN(b64) = 0 THEN
    BrowserDownloadPdf = "ERROR: download fallito da " + url
    EXIT FUNCTION
  END IF
  path = destDir + "/" + fileName
  OPEN path FOR BINARY AS #1
  PUT #1, , FROMBASE64(b64)
  CLOSE #1
  text = CONVERT.ToText(path)
  IF LEN(text) = 0 THEN
    BrowserDownloadPdf = "ERROR: estrazione testo fallita da " + path
    EXIT FUNCTION
  END IF
  BrowserDownloadPdf = text
END FUNCTION
DIM text AS STRING
DIM r$ AS STRING

PRINT "BrowserD4: avvio"

REM download (base64 via FETCH) + estrazione testo
text = BrowserDownloadPdf(9222, "http://127.0.0.1:8104/d4.pdf.b64", "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Community/Browser/work", "d4.pdf")
PRINT "D4 textlen:"; LEN(text)
IF LEN(text) > 10 THEN
  PRINT "D4 estrazione: ok"
ELSE
  PRINT "D4 estrazione: FALLITO"
END IF

REM ingestion → RAG (vector DB)
SQL OPEN ":memory:" ALIAS D4Db
AI RAG ENSURE COLLECTION "D4Db", "pdfdocs", 8
AI RAG INDEX TEXT "D4Db", "pdfdocs", "d4doc", text
PRINT "D4 index: ok"

REM ricerca end-to-end
r$ = AI.SEARCH("D4Db", "pdfdocs", "browser automation", 3)
PRINT "D4 search:"; MID$(r$, 1, 120)
IF INSTR(r$, "d4doc") > 0 THEN
  PRINT "D4 RAG: ok"
ELSE
  PRINT "D4 RAG: FALLITO"
END IF

PRINT "BrowserD4: fine"
