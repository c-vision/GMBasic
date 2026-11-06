' EXPECT: dimdyn=00ff10aa7f80|dimfix=00ff10aa|param=00ff10aa7f80
' MiniTest ENCODING: TOHEX su array BYTE — fissa la regressione riparata nell'emitter C99
' (2026-11-06). Copre i tre percorsi di TOHEX su un array NON stringa:
'   1. array dinamico `DIM d() AS BYTE`            -> bound `<nome>_len`;
'   2. array a bound fisso `DIM k(n) AS BYTE`      -> bound `<nome>__b0 + 1`;
'   3. PARAMETRO `a() AS BYTE` dentro una SUB      -> `elem_byte` sul parametro.
' Senza il ramo `gmb_hex_from_doubles` l'operando finiva in `gmb_double_to_string(puntatore)`
' (C non compilabile); senza `elem_byte` sul parametro la regressione restava nel callee.
' I valori includono 0x00 in testa (nessun troncamento a NUL) e byte > 127.
IMPORT CRYPTOGRAPHY

SUB MostraParam(a() AS BYTE)
  PRINT "param="; TOHEX(a)
END SUB

SUB Main()
  DIM d() AS BYTE = FROMHEX("00ff10aa7f80")
  PRINT "dimdyn="; TOHEX(d)

  DIM k(3) AS BYTE
  k(0) = 0
  k(1) = 255
  k(2) = 16
  k(3) = 170
  PRINT "dimfix="; TOHEX(k)

  MostraParam(d)
END SUB
