' MiniTest NEGATIVO FFI (F5): BYREF di STRING non è una firma valida per il marshalling.
' La DECLARE deve essere rifiutata con lo STESSO messaggio sui 4 backend (parità errori).
' EXPECT: COMPILE ERROR: FFI 'neg_byref_str': BYREF di STRING non supportato dal marshalling FFI (passare BYVAL).
DECLARE SUB neg_byref_str LIB "libvec_module" (BYREF s AS STRING)

SUB Main()
    DIM s AS STRING
    neg_byref_str(s)
END SUB
