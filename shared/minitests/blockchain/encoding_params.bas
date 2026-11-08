' EXPECT: b58=Cn8eVZg|b32=NBSWY3DP|b58d=68656c6c6f|b32d=68656c6c6f|rlp=83646f67|v=ac02|dv= 300
' MiniTest BLOCKCHAIN (pacchetto FFI): TUTTE le encoding del pacchetto — base58, base32, RLP,
' varint — attraversano un PARAMETRO `x() AS BYTE` di una SUB prima di arrivare al wrapper
' `blockchain.inc`. Copre encode E decode: l'array non e' un DIM locale del chiamante ma il
' parametro della SUB (percorso array→parametro→FFI, dove in C99 l'array non ha `.data`/`.length`).
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB EncB58(a() AS BYTE)
  PRINT "b58="; BASE58ENCODE(a)
END SUB

SUB DecB58(t AS STRING, o() AS BYTE)
  DIM n AS LONG
  n = BASE58DECODE(t, o)
  PRINT "b58d="; LEFT(TOHEX(o), n * 2)
END SUB

SUB EncB32(a() AS BYTE)
  PRINT "b32="; BASE32ENCODE(a)
END SUB

SUB DecB32(t AS STRING, o() AS BYTE)
  DIM n AS LONG
  n = BASE32DECODE(t, o)
  PRINT "b32d="; LEFT(TOHEX(o), n * 2)
END SUB

SUB EncRlp(a() AS BYTE, o() AS BYTE)
  DIM n AS LONG
  n = RLPENCODE(a, o)
  PRINT "rlp="; LEFT(TOHEX(o), n * 2)
END SUB

SUB EncVar(v AS LONG, o() AS BYTE)
  DIM n AS LONG
  n = VARINTENCODE(v, o)
  PRINT "v="; LEFT(TOHEX(o), n * 2)
END SUB

SUB DecVar(a() AS BYTE)
  PRINT "dv="; VARINTDECODE(a)
END SUB

SUB Main()
  DIM d() AS BYTE = FROMHEX("68656c6c6f")
  EncB58(d)
  EncB32(d)

  DIM o(63) AS BYTE
  DecB58("Cn8eVZg", o)
  DecB32("NBSWY3DP", o)

  DIM dog() AS BYTE = FROMHEX("646f67")
  EncRlp(dog, o)

  DIM vb(9) AS BYTE
  EncVar(300, vb)

  DIM c() AS BYTE = FROMHEX("ac02")
  DecVar(c)
END SUB
