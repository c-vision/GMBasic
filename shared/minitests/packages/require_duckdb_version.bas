' TAG: unsupported-c99native
' EXPECT: count= 3
' REQUIRE con VERSION esplicito (sintassi canonica, 2026-10-06) + FFI DuckDB su 4 backend.
'   REQUIRE "duckdb" VERSION "^1.5"   -> il vincolo e' valutato con SemVer sulle versioni in
'                                       cache; senza VERSION si prende la piu' recente.
'   IMPORT DUCKDB WITH ALIAS          -> il namespace in scope -> DUCKDB.<Membro>(...).
' Questo test prova che la FORMA CANONICA e' accettata e risolta su tutte e 5 le gambe, e che
' l'output non cambia rispetto a require_duckdb.bas (REQUIRE senza versione).
' Precisazione onesta: con UNA sola versione in cache (~/.gmbasic/packages/duckdb = 1.5.6) il vincolo
' "^1.5" non e' realmente esercitato — anche un resolver che IGNORASSE il vincolo passerebbe.
' La SEMANTICA del vincolo (e il confronto SemVer, non lessicografico) e' provata su una cache a
' piu' versioni da tools/test_require_version_parity.sh, che confronta i due mondi C# e C.
REQUIRE "duckdb" VERSION "^1.5"
IMPORT DUCKDB WITH ALIAS

SUB Main()
    DIM h AS LONG
    DIM app AS LONG
    DIM n AS LONG
    IF DUCKDB.AVAILABLE() = 0 THEN
        PRINT "count= 0"
        EXIT SUB
    END IF
    h = DUCKDB.OPEN("")
    DUCKDB.EXEC(h, "CREATE TABLE t(ts BIGINT, sid INTEGER, o DOUBLE, h DOUBLE, l DOUBLE, c DOUBLE, v DOUBLE)")
    app = DUCKDB.APPENDER(h, "t")
    DUCKDB.APPEND(app, 1000, 1, "1,2,0,1,10")
    DUCKDB.APPEND(app, 1060, 1, "1,2,0,1,10")
    DUCKDB.APPEND(app, 1120, 2, "1,2,0,1,10")
    DUCKDB.FLUSH(app)
    DUCKDB.CLOSEAPPENDER(app)
    IF DUCKDB.QUERY(h, "SELECT count(*) FROM t", n) = 0 THEN PRINT "count="; n
    DUCKDB.CLOSE(h)
END SUB