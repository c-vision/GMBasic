' FILE: Connectors/ConnectorCalendar.bas
' Calendar (google-calendar, T0) — OAuth + events, auto-discovery

IMPORT AI
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP
IMPORT OAUTH

SUB Main()
    PRINT "=== Calendar Connector ==="
    AI CONNECTOR LOAD "google-calendar"
    PRINT "LOAD google-calendar OK"

    ' OAuth (simulato, in prod PKCE flow)
    AI CONNECTOR AUTH "google-calendar", "{""access_token"":""ya29.cal_demo"",""refresh_token"":""1//cal""}"
    PRINT "AUTH calendar OK"

    DIM ev$ AS STRING = AI.CONNECTORCALL("google-calendar", "events", "{}")
    PRINT "calendar.events: "; LEFT(ev$, 400)

    DIM li$ AS STRING = AI.CONNECTORCALL("google-calendar", "list", "{}")
    PRINT "calendar.list: "; LEFT(li$, 300)

    PRINT "=== Calendar OK (F1 reale se token valido, altrimenti stub) ==="
END SUB
