' Sample pacchetto duckdb: usa il pacchetto via REQUIRE + IMPORT (gli .inc sono in includes/).
REQUIRE "duckdb"
IMPORT DUCKDB WITH ALIAS

SUB Main()
    PRINT "duckdb available="; DUCKDB.AVAILABLE()
END SUB
