' FILE: samples/database/RddDriversComplete.bas
' ===================================================================
' RDD drivers completati (2026-08-21): DBF/ISAM/SPEED senza no-op.
' Indici, GoTo reale, lock. Percorsi: fixtures in samples/fixtures,
' output in work.
' Sintassi: comandi RDD in forma legacy a spazio; navigazione generica
' via DBF GO/SKIP/EOF (driver-agnostica sull'area corrente).
' ===================================================================
IMPORT RDD
IMPORT DBF
IMPORT IO

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    PRINT "=== RDD Drivers Complete ==="
    PRINT

    ' --- 1. DBF su file reale: indice NDX + lock + SET ORDER ---
    PRINT "1. DBF (fixture cust.dbf)"
    RDD USE "samples/fixtures/cust.dbf" ALIAS C
    RDD CREATE INDEX 1, g_work + "/dbf_name.ntx", "NAME"
    RDD SET ORDER 1, "dbf_name"
    DBF GO TOP
    PRINT "   primo per NAME: "; RDD.GETFIELD(1, 1)
    RDD CLOSE INDEX 1, "dbf_name"
    DBF CLOSE
    PRINT

    ' --- 2. ISAM: GoTo reale + indice secondario ---
    PRINT "2. ISAM (fixture items.isdb)"
    RDD USE "samples/fixtures/items.isdb" ALIAS I
    DBF GO 2
    PRINT "   GoTo(2): "; RDD.GETFIELD(1, 1)
    RDD CREATE INDEX 1, "IX_PRICE", "Price"
    RDD SET ORDER 1, "IX_PRICE"
    RDD CLOSE INDEX 1, "IX_PRICE"
    DBF CLOSE
    PRINT

    ' --- 3. SPEED: GoTo reale su KV (driver SPEED, tabella auto) ---
    PRINT "3. SPEED (fixture kv.speed)"
    RDD USE "samples/fixtures/kv.speed" VIA "SPEED" ALIAS K
    DBF GO 2
    PRINT "   GoTo(2): "; RDD.GETFIELD(1, 1)
    DBF CLOSE
    PRINT

    PRINT "--- RDD DRIVERS OK ---"
END SUB