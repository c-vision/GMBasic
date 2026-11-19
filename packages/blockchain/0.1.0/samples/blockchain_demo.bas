' blockchain_demo.bas — pacchetto `blockchain`: dominio BLOCKCHAIN via FFI.
' I byte viaggiano in ARRAY BYTE: i buffer di uscita li dimensiona il chiamante e i
' wrapper deducono la lunghezza da LEN(a()); l'esadecimale si rende con BYTESHEX.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

SUB Main()
  ' hash di "hello" (input BYTE; Keccak/BLAKE2b 32 byte, RIPEMD-160 20 byte)
  DIM msg(4) AS BYTE
  DIM nMsg AS LONG = CRYPTOGRAPHY.FROMHEX("68656c6c6f", msg)
  DIM hk(31) AS BYTE
  DIM hr(19) AS BYTE
  BLOCKCHAIN.KECCAK256(msg, hk)
  BLOCKCHAIN.RIPEMD160(msg, hr)
  PRINT "keccak256('hello') = "; CRYPTOGRAPHY.BYTESHEX(hk)
  PRINT "ripemd160('hello') = "; CRYPTOGRAPHY.BYTESHEX(hr)

  ' base58 del byte 0x00
  DIM zero(0) AS BYTE
  zero(0) = 0
  PRINT "base58(00) = "; BLOCKCHAIN.BASE58ENCODE(zero)

  ' chiave privata 1 -> G compresso (33 byte)
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM pub(32) AS BYTE
  DIM nPub AS LONG = BLOCKCHAIN.GETPUBLICKEY(priv, pub)
  PRINT "pub(1) = "; LEFT(CRYPTOGRAPHY.BYTESHEX(pub), nPub * 2)

  ' indirizzo ETH: la funzione hasha i byte ricevuti, quindi passo la chiave NON compressa
  DIM full(64) AS BYTE
  DIM nFull AS LONG = CRYPTOGRAPHY.FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798" + _
                                            "483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8", full)
  DIM addr$ AS STRING
  addr$ = BLOCKCHAIN.ETHADDRFROMPUB(full)
  PRINT "ethaddr = "; addr$
  PRINT "checksum = "; BLOCKCHAIN.ETHADDRCHECKSUM(addr$)
  PRINT "btc = "; BLOCKCHAIN.BTCADDRFROMPUB(pub, 0)

  ' varint 300 = AC 02
  DIM vi(4) AS BYTE
  DIM nVi AS LONG = BLOCKCHAIN.VARINTENCODE(300, vi)
  PRINT "varint(300) = "; LEFT(CRYPTOGRAPHY.BYTESHEX(vi), nVi * 2)
  PRINT "varint-back = "; BLOCKCHAIN.VARINTDECODE(vi)
END SUB
