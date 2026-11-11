' MiniTest NEGATIVO FFI: il BYREF scalare ammette solo tipi a 8 byte; BYTE/SBYTE sono
' ammessi come ELEMENTO di un array (scratch a 8 byte), non come cella puntata.
' La DECLARE deve essere rifiutata con lo stesso messaggio sulle 4 gambe.
' EXPECT: ERROR: FFI 'neg_byref_byte': tipo non supportato per il parametro BYREF 'b' (solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).
DECLARE SUB neg_byref_byte LIB "libvec_module" (BYREF b AS BYTE)

SUB Main()
    DIM b AS BYTE
    neg_byref_byte(b)
END SUB
