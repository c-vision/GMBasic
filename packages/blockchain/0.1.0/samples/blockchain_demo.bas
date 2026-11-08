' blockchain_demo.bas — pacchetto `blockchain`: dominio BLOCKCHAIN via FFI.
' I byte viaggiano in HEX (gli array BYTE non sono ammessi come elementi-array FFI).
REQUIRE "blockchain"
IMPORT BLOCKCHAIN WITH ALIAS

SUB Main()
  ' hash del testo vuoto (vettore noto)
  PRINT "keccak256('') = "; BLOCKCHAIN.KECCAK256("")
  PRINT "ripemd160('') = "; BLOCKCHAIN.RIPEMD160("")
  ' base58 del byte 0x00
  PRINT "base58(00) = "; BLOCKCHAIN.BASE58ENCODE("00")
  ' chiave privata 1 -> G compresso (33 byte)
  DIM priv$ AS STRING
  priv$ = "0000000000000000000000000000000000000000000000000000000000000001"
  DIM pub$ AS STRING
  pub$ = BLOCKCHAIN.GETPUBLICKEY(priv$)
  PRINT "pub(1) = "; pub$
  ' indirizzo ETH: la funzione hasha i byte ricevuti, quindi passo la chiave NON compressa
  DIM pubFull$ AS STRING
  pubFull$ = "0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798" + _
             "483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8"
  DIM addr$ AS STRING
  addr$ = BLOCKCHAIN.ETHADDRFROMPUB(pubFull$)
  PRINT "ethaddr = "; addr$
  PRINT "checksum = "; BLOCKCHAIN.ETHADDRCHECKSUM(addr$)
  PRINT "btc = "; BLOCKCHAIN.BTCADDRFROMPUB(pub$, 0)
  ' varint
  PRINT "varint(300) = "; BLOCKCHAIN.VARINTENCODE(300)
  PRINT "varint-back = "; BLOCKCHAIN.VARINTDECODE(BLOCKCHAIN.VARINTENCODE(300))
END SUB
