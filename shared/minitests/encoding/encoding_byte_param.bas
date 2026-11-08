' EXPECT: b64=aGVsbG8=|fix=aGVsbG8=|fromhex=aGVsbG8=
' MiniTest ENCODING: TOBASE64 e FROMHEX con array BYTE passati come PARAMETRO `a() AS BYTE`
' (regressione emitter C99, 2026-11-06). In C un array BYTE (DIM o parametro) è
' `gmb_double_t*` + bound, senza `.data`/`.length`: senza il ponte `gmb_bytes_from_doubles`
' l'encoder riceveva il puntatore da `gmb_double_to_string` (C non compilabile).
'   - `fix`: parametro da un buffer a bound fisso `DIM k(4) AS BYTE`;
'   - `fromhex`: FROMHEX dentro una SUB, il cui risultato entra come array BYTE.
' BASE58ENCODE/BASE32ENCODE NON sono usati qui: sul C99 non sono nomi risolvibili
' (registrazioni rimosse con la surface BLOCKCHAIN, vedi MiniTest blockchain/encoding_params.bas).
IMPORT CRYPTOGRAPHY

SUB EncB64(etichetta AS STRING, a() AS BYTE)
  PRINT etichetta; TOBASE64(a)
END SUB

SUB DaHex(s AS STRING)
  DIM b() AS BYTE = FROMHEX(s)
  PRINT "fromhex="; TOBASE64(b)
END SUB

SUB Main()
  DIM d() AS BYTE = FROMHEX("68656c6c6f")
  EncB64("b64=", d)

  DIM k(4) AS BYTE
  k(0) = 104
  k(1) = 101
  k(2) = 108
  k(3) = 108
  k(4) = 111
  EncB64("fix=", k)

  DaHex("68656c6c6f")
END SUB
