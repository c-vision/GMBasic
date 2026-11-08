' blockchain_bytes.bas — pacchetto `blockchain`: dominio BLOCKCHAIN via FFI con ARRAY DI BYTE.
' I buffer di uscita li dimensiona il chiamante; i wrapper deducono le lunghezze con UBOUND.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN WITH ALIAS
IMPORT CRYPTOGRAPHY

SUB Main()
  ' Keccak-256 di "abc"
  DIM d(2) AS BYTE
  d(0) = 97 : d(1) = 98 : d(2) = 99
  DIM h(31) AS BYTE
  BLOCKCHAIN.KECCAK256(d, h)
  PRINT "keccak="; CRYPTOGRAPHY.TOHEX(h)

  ' chiave privata 1 -> G compresso (33 byte)
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM pub(32) AS BYTE
  PRINT "publen="; BLOCKCHAIN.GETPUBLICKEY(priv, pub)
  PRINT "pub="; CRYPTOGRAPHY.TOHEX(pub)

  ' indirizzo ETH dalla chiave NON compressa (la funzione hasha i byte ricevuti)
  DIM full() AS BYTE = CRYPTOGRAPHY.FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798" + _
                                             "483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8")
  DIM addr$ AS STRING
  addr$ = BLOCKCHAIN.ETHADDRFROMPUB(full)
  PRINT "eth="; addr$
  PRINT "cks="; BLOCKCHAIN.ETHADDRCHECKSUM(addr$)

  ' base58 dei soli byte di indirizzo (nessun checksum)
  PRINT "b58="; BLOCKCHAIN.BASE58ENCODE(d)

  ' varint 300 = AC 02
  DIM v(1) AS BYTE
  PRINT "vlen="; BLOCKCHAIN.VARINTENCODE(300, v)
  PRINT "vhex="; CRYPTOGRAPHY.TOHEX(v)
  PRINT "vback="; BLOCKCHAIN.VARINTDECODE(v)
END SUB
