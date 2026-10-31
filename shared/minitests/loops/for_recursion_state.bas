' EXPECT: n= 2  i= 1|n= 1  i= 1|n= 1  i= 2|n= 1  i= 3|done n= 1|n= 2  i= 2|n= 2  i= 3|done n= 2
' MiniTest: ricorsione DENTRO un ciclo FOR — quando la chiamata annidata ritorna, il ciclo
' del chiamante deve continuare dal valore successivo.
' Era un bug su PCode e VM/C: lo stato del FOR era per TASK e indicizzato SOLO dallo slot,
' non per frame. Il callee chiama OP_FOR_PREPARE sullo stesso slot e sovrascrive
' counter/end/step; a fine ciclo fa Remove(slot) (PCode) / for_active[slot]=0 (VM/C), così
' il FOR del chiamante non trovava piu' il suo stato e terminava: si perdevano le iterazioni
' 2 e 3 (il 2 e il 3 esterni).
'   PCode: CORRETTO 2026-10-28 — lo stato vive sul frame (PCodeCallFrame.ForState, helper
'          PCodeProcessor.ForStateOf) invece che sul task indicizzato per slot.
'   VM/C:  CORRETTO 2026-10-28 — gmb_frame_t.for_state (gmb_for_state_t) + for_state_of()
'          in gmb_vm_core.c: OP_FOR_PREPARE/OP_FOR_LOOP, OP_NATIVE_LOOP_ACC,
'          OP_NATIVE_STRING_APPEND_LOOP e L_FAST_FOR_LOOP usano lo stato del frame.
' IL e C99Native: gia' corretti (questo EXPECT e' l'output di IL).
' Sintomo reale che l'ha scoperto: la visita ricorsiva di una directory si fermava al primo
' sottodirectory (1 file su 1.208) in tools/find_illegal_escapes.bas.
SUB R(n AS LONG)
  DIM i AS LONG
  FOR i = 1 TO 3
    PRINT "n="; n; " i="; i
    IF n > 1 AND i = 1 THEN R(n - 1)
  NEXT i
  PRINT "done n="; n
END SUB
SUB Main()
  R(2)
END SUB
