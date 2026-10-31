' TAG: unsupported-c99native
' EXPECT: count= 3
' REQUIRE (package manager) + FFI DuckDB (namespace DUCKDB) su 4 backend.
'   REQUIRE "duckdb"  risolve il pacchetto (~/.gmb/packages) e include duckdb.inc.
'   IMPORT DUCKDB WITH ALIAS  porta il namespace in scope -> chiamate DUCKDB.<Membro>(...).
' Copre: DUCKDB.AVAILABLE/OPEN/EXEC/QUERY/APPENDER/APPEND/FLUSH/CLOSEAPPENDER/CLOSE.
' Parità 5/5: IL, P-Code, VM/C, C99Native.
REQUIRE "duckdb"
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
