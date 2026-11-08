' memory.inc — namespace MEMORY via FFI (lib `memory`, libgmbmemory).
'
' KV store della memoria AI su SQLite in-memory: STORE / LOAD / SEARCH / LIST / DELETE.
' `value` è TESTO (stringa). Il motore la salva come stringa JSON e LOAD la
' restituisce identica; per JSON strutturato passare la stringa JSON.
'
' Uso: IMPORT MEMORY   (shared/libs: nessun REQUIRE; export in shared/include)

NAMESPACE MEMORY

DECLARE NATIVE FUNCTION MEMMSTORE LIB "gmbmemory" ALIAS "ofx_mem_store" (key AS STRING, value AS STRING, longTerm AS LONG) AS LONG
DECLARE NATIVE FUNCTION MEMMLOAD LIB "gmbmemory" ALIAS "ofx_mem_load" (key AS STRING) AS STRING
DECLARE NATIVE FUNCTION MEMMSEARCH LIB "gmbmemory" ALIAS "ofx_mem_search" (pattern AS STRING) AS STRING
DECLARE NATIVE FUNCTION MEMMDELETE LIB "gmbmemory" ALIAS "ofx_mem_delete" (key AS STRING) AS LONG
DECLARE NATIVE FUNCTION MEMMLIST LIB "gmbmemory" ALIAS "ofx_mem_list" (dummy AS LONG) AS STRING

' STORE(key$, value$, longTerm) -> BIT (-1 scritto, 0 non scritto)
PUBLIC FUNCTION STORE(key AS STRING, value AS STRING, longTerm AS LONG) AS LONG
    RETURN (MEMMSTORE(key, value, longTerm) <> 0)
END FUNCTION

' LOAD(key$) -> valore ("" se assente)
PUBLIC FUNCTION LOAD(key AS STRING) AS STRING
    RETURN MEMMLOAD(key)
END FUNCTION

' SEARCH(pattern$) -> chiavi che contengono pattern, separate da CHR(10)
PUBLIC FUNCTION SEARCH(pattern AS STRING) AS STRING
    RETURN MEMMSEARCH(pattern)
END FUNCTION

' DELETE(key$) -> BIT (-1 eliminata, 0 non presente)
PUBLIC FUNCTION DELETE(key AS STRING) AS LONG
    RETURN (MEMMDELETE(key) <> 0)
END FUNCTION

' LIST() -> chiavi ordinate, separate da CHR(10)
PUBLIC FUNCTION LIST() AS STRING
    RETURN MEMMLIST(0)
END FUNCTION
