' EXPECT: priv=cd99dfddcc661bd850c11c510a05e52e2327aa662bc97772ba0e46c2e074f5db|chain=76b421d62999fb62022cc8f01200bb31a169daf72673eba7e58442b8b67cbfc9|match=SI
' MiniTest BLOCKCHAIN (pacchetto FFI): DERIVEKEY(seed 0..31) scrive 97 byte nel buffer
' (priv 32 || pub 33 || chainCode 32); il path NON e' applicato (chiave master, come il
' builtin dismesso) e la chiave pubblica ricavata da GETPUBLICKEY combacia.
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM seed(31) AS BYTE
  DIM _n_seed AS LONG
  _n_seed = FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", seed)
  DIM dk(96) AS BYTE
  DIM n AS LONG
  n = DERIVEKEY(seed, "m/44'/60'/0'/0/0", dk)
  DIM priv(31) AS BYTE
  DIM pub(32) AS BYTE
  DIM chain(31) AS BYTE
  DIM i AS LONG
  FOR i = 0 TO 31
    priv(i) = dk(i)
    chain(i) = dk(65 + i)
  NEXT i
  FOR i = 0 TO 32
    pub(i) = dk(32 + i)
  NEXT i
  PRINT "priv="; BYTESHEX(priv)
  PRINT "chain="; BYTESHEX(chain)
  DIM re(63) AS BYTE
  n = GETPUBLICKEY(priv, re)
  IF LEFT(BYTESHEX(re), n * 2) = BYTESHEX(pub) THEN
    PRINT "match=SI"
  ELSE
    PRINT "match=NO"
  END IF
END SUB
