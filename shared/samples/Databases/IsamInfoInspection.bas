' ==============================================================================
' IsamInfoInspection.bas — ISAM.INFO: JSON d'ispezione header+schema (parità 4 backend)
' ==============================================================================
' Copre: ISAM.INFO(alias) — file, size_bytes, format_version, type_name, record_size,
'        active_records, primary_key_root, schema_offset/length, secondary_indexes,
'        field_names. Il file creato è byte-identico tra engine C e C#.
' ==============================================================================
IMPORT ISAM

TYPE Riga
    ID AS STRING * 10
    Valore AS STRING * 20
END TYPE

SUB Main()
    ON ERROR RESUME NEXT
    KILL "/private/tmp/isam_info_demo.isdb"
    ON ERROR GOTO 0
    ISAM OPEN "/private/tmp/isam_info_demo.isdb" OF Riga ALIAS X INDEXED BY ID
    PRINT "=== ISAM.INFO ==="
    PRINT ISAM.INFO(X)
    PRINT
    PRINT "Completato."
    ISAM CLOSE X
END SUB
