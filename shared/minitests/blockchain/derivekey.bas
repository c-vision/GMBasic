' EXPECT: priv=cd99dfddcc661bd850c11c510a05e52e2327aa662bc97772ba0e46c2e074f5db|chain=76b421d62999fb62022cc8f01200bb31a169daf72673eba7e58442b8b67cbfc9|match=SI
' MiniTest BLOCKCHAIN: DERIVEKEY(seed 0..31, path) = master HMAC-SHA512 (il path e' ignorato su tutti i backend) + round-trip GETPUBLICKEY.
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
SUB Main()
  DIM seed() = FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
  DIM dk = DERIVEKEY(seed(), "m/44'/60'/0'/0/0")
  DIM priv() = dk("privateKey")
  DIM chain() = dk("chainCode")
  DIM pub() = dk("publicKey")
  PRINT "priv="; TOHEX(priv())
  PRINT "chain="; TOHEX(chain())
  DIM re() = GETPUBLICKEY(priv())
  IF TOHEX(re()) = TOHEX(pub()) THEN
    PRINT "match=SI"
  ELSE
    PRINT "match=NO"
  END IF
END SUB
