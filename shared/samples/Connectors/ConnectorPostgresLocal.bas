' FILE: Connectors/ConnectorPostgresLocal.bas
' Esempio Postgres locale — DATABASE_URL=postgresql://gaetano@127.0.0.1:5432/
' Configurazione da BASIC via AI CONNECTOR (F1, Npgsql, postgres T0)

IMPORT AI

SUB Main()
    PRINT "=== Postgres Locale DATABASE_URL ==="
    PRINT "URL: postgresql://gaetano@127.0.0.1:5432/"

    ' 1. Load postgres (marketplace T0, auth none, tools query/execute/schema)
    AI CONNECTOR LOAD "postgres"
    PRINT "LOAD postgres OK"

    ' 2. Configura da BASIC — la conn_string è persistita in config/connectors.json
    ' Npgsql accetta sia URL postgresql:// sia Host=...;Port=...;Username=...
    ' Usiamo l'URL diretto (testa anche la conversione manuale se il driver non lo supporta)
    AI CONNECTOR SECRET SET "postgres", "conn_string", "postgresql://gaetano@127.0.0.1:5432/"
    PRINT "SECRET conn_string = postgresql://gaetano@127.0.0.1:5432/ OK (convertito in Host=...)"

    DIM sec$ AS STRING = AI.CONNECTORSECRETGET("postgres", "conn_string")
    PRINT "SECRET GET: "; sec$

    DIM st$ AS STRING = AI.CONNECTORSTATUS("postgres")
    PRINT "STATUS: "; LEFT(st$, 300)

    ' 3. Prova connessione (richiede server esterno: docker run -p 5432:5432 postgres)
    ON ERROR GOTO ConnErr
    DIM ok$ AS STRING = AI.CONNECTORCALL("postgres", "connect", "{}")
    PRINT "CONNECT OK: "; LEFT(ok$, 300)
    GOTO AfterConn
ConnErr:
    PRINT "CONNECT FAILED: "; ERROR$
    PRINT "SUGGERIMENTO: avvia postgres locale: docker run --name gmb-pg -e POSTGRES_HOST_AUTH_METHOD=trust -p 5432:5432 -d postgres:15"
    PRINT "Oppure crea l'utente gaetano: psql -c ""CREATE USER gaetano WITH SUPERUSER;"" -c ""CREATE DATABASE gaetano OWNER gaetano;"""
    RESUME AfterConn
AfterConn:
    ON ERROR GOTO 0

    ' 4. Prova query di sistema (se connesso)
    ON ERROR GOTO QueryErr
    DIM ver$ AS STRING = AI.CONNECTORCALL("postgres", "query", "{""sql"":""SELECT version()""}")
    PRINT "SELECT version(): "; LEFT(ver$, 400)

    DIM dbs$ AS STRING = AI.CONNECTORCALL("postgres", "query", "{""sql"":""SELECT datname FROM pg_database LIMIT 3""}")
    PRINT "pg_database: "; LEFT(dbs$, 400)

    ' Test tabella demo (crea se non esiste, inserisce, legge)
    DIM cr$ AS STRING = AI.CONNECTORCALL("postgres", "execute", "{""sql"":""CREATE TABLE IF NOT EXISTS gmb_demo (id SERIAL PRIMARY KEY, name TEXT)""}")
    PRINT "CREATE TABLE gmb_demo: "; LEFT(cr$, 200)
    DIM ins$ AS STRING = AI.CONNECTORCALL("postgres", "execute", "{""sql"":""INSERT INTO gmb_demo (name) VALUES ('Alice') ON CONFLICT DO NOTHING""}")
    PRINT "INSERT Alice: "; LEFT(ins$, 200)
    DIM sel$ AS STRING = AI.CONNECTORCALL("postgres", "query", "{""sql"":""SELECT * FROM gmb_demo LIMIT 2""}")
    PRINT "SELECT gmb_demo: "; LEFT(sel$, 400)
    GOTO AfterQuery
QueryErr:
    PRINT "QUERY FAILED: "; ERROR$
    RESUME AfterQuery
AfterQuery:
    ON ERROR GOTO 0

    ' 5. Schema
    ON ERROR GOTO SchemaErr
    DIM sch$ AS STRING = AI.CONNECTORCALL("postgres", "schema", "{}")
    PRINT "SCHEMA: "; LEFT(sch$, 500)
    GOTO AfterSchema
SchemaErr:
    PRINT "SCHEMA FAILED: "; ERROR$
    RESUME AfterSchema
AfterSchema:
    ON ERROR GOTO 0

    PRINT "=== Postgres Locale Test Completato ==="
END SUB
