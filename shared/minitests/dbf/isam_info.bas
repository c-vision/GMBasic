' MiniTest: ISAM.INFO — JSON header+schema, parita' 4 backend (file ISAM byte-identico).
' EXPECT: {"file":"/private/tmp/gmb_mt_isaminfo.isdb","size_bytes":8286,"format_version":1,"type_name":"R","record_size":55,"active_records":0,"last_rec_number":0,"primary_key_root":-1,"lob_heap_start":8286,"next_available_page":8286,"free_list_head":-1,"page_free_list_head":-1,"schema_offset":8192,"schema_length":79,"secondary_indexes":1,"durability_mode":"Full","locking_strategy":"IntraProcess","field_names":["ID","V"]}
IMPORT ISAM
TYPE R
    ID AS STRING * 10
    V AS STRING * 20
END TYPE
SUB Main()
    ON ERROR RESUME NEXT
    KILL "/private/tmp/gmb_mt_isaminfo.isdb"
    ON ERROR GOTO 0
    ISAM OPEN "/private/tmp/gmb_mt_isaminfo.isdb" OF R ALIAS X INDEXED BY ID
    PRINT ISAM.INFO(X)
    ISAM CLOSE X
END SUB
