' FILE: Connectors/ConnectorT1_SheetsNotionLinear.bas
' T1 (12) — google-sheets, notion, linear, jira via HTTP generico F2 (reale se Bearer)

IMPORT AI

SUB Main()
    PRINT "=== T1 Sheets/Notion/Linear/Jira ==="

    ' google-sheets (OAuth)
    AI CONNECTOR LOAD "google-sheets"
    AI CONNECTOR SECRET SET "google-sheets", "api_key", "demo"
    DIM sh$ AS STRING = AI.CONNECTORCALL("google-sheets", "read", "{""spreadsheetId"":""abc"",""range"":""A1:B2""}")
    PRINT "sheets.read: "; LEFT(sh$, 300)

    ' notion (api_key)
    AI CONNECTOR LOAD "notion"
    AI CONNECTOR SECRET SET "notion", "api_key", "secret_demo"
    DIM no$ AS STRING = AI.CONNECTORCALL("notion", "search", "{""query"":""architettura""}")
    PRINT "notion.search: "; LEFT(no$, 300)

    ' linear (apikey, GraphQL POST)
    AI CONNECTOR LOAD "linear"
    AI CONNECTOR SECRET SET "linear", "api_key", "lin_demo"
    DIM li$ AS STRING = AI.CONNECTORCALL("linear", "issues", "{""team"":""ENG""}")
    PRINT "linear.issues: "; LEFT(li$, 300)

    ' jira (oauth)
    AI CONNECTOR LOAD "jira"
    AI CONNECTOR SECRET SET "jira", "api_key", "jira_demo"
    DIM ji$ AS STRING = AI.CONNECTORCALL("jira", "issues", "{""project"":""GMB""}")
    PRINT "jira.issues: "; LEFT(ji$, 300)

    ' onedrive (Microsoft Graph)
    AI CONNECTOR LOAD "onedrive"
    DIM od$ AS STRING = AI.CONNECTORCALL("onedrive", "list", "{""path"":""/""}")
    PRINT "onedrive.list: "; LEFT(od$, 200)

    PRINT "=== T1 OK (F2 HTTP generico, 200 se token reale) ==="
END SUB
