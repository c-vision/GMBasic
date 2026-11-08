' EXPECT: publen= 33|priv=cd99dfddcc661bd850c11c510a05e52e2327aa662bc97772ba0e46c2e074f5db|chain=76b421d62999fb62022cc8f01200bb31a169daf72673eba7e58442b8b67cbfc9|match=SI|seed=c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04|sigprefix=30|derlen>=68=-1|diff= 5
' MiniTest BLOCKCHAIN (pacchetto FFI): DERIVEKEY / GETPUBLICKEY / MNEMONICTOSEED /
' SECP256K1SIGN / BLOCKDIFFICULTY con gli array passati come PARAMETRI `x() AS BYTE` di SUB,
' non come DIM locali del chiamante. Ogni wrapper costruisce `n = LEN(..)` e `cap = LEN(out)`
' sugli array di parametro (blockchain.inc:78-103): se quelle lunghezze non arrivassero,
' la lib scriverebbe su una copia o rifiuterebbe l'output. `DeriveAndPub` esercita inoltre il
' passaggio parametro -> parametro (seed/dk/re inoltrati a Derive e Pub).
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB Derive(seed() AS BYTE, dk() AS BYTE)
  DIM n AS LONG
  n = DERIVEKEY(seed, "m/44'/60'/0'/0/0", dk)
END SUB

SUB Pub(priv() AS BYTE, o() AS BYTE)
  DIM n AS LONG
  n = GETPUBLICKEY(priv, o)
  PRINT "publen="; n
END SUB

SUB DeriveAndPub(seed() AS BYTE, dk() AS BYTE, re() AS BYTE)
  DIM p(31) AS BYTE
  DIM i AS LONG
  Derive(seed, dk)
  FOR i = 0 TO 31
    p(i) = dk(i)
  NEXT i
  Pub(p, re)
END SUB

SUB MnToSeed(m$ AS STRING, pass AS STRING, o() AS BYTE)
  DIM n AS LONG
  n = MNEMONICTOSEED(m$, pass, o)
END SUB

SUB Sign(msg() AS BYTE, priv() AS BYTE, o() AS BYTE)
  DIM n AS LONG
  n = SECP256K1SIGN(msg, priv, o)
  PRINT "sigprefix="; LEFT(TOHEX(o), 2)
  PRINT "derlen>=68="; (n >= 68)
END SUB

SUB Diff(h() AS BYTE)
  PRINT "diff="; BLOCKDIFFICULTY(h, 553648127)
END SUB

SUB Main()
  DIM seed() AS BYTE = FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
  DIM dk(96) AS BYTE
  DIM re(63) AS BYTE
  DeriveAndPub(seed, dk, re)

  DIM priv(31) AS BYTE
  DIM chain(31) AS BYTE
  DIM pub(32) AS BYTE
  DIM i AS LONG
  FOR i = 0 TO 31
    priv(i) = dk(i)
    chain(i) = dk(65 + i)
  NEXT i
  FOR i = 0 TO 32
    pub(i) = dk(32 + i)
  NEXT i
  PRINT "priv="; TOHEX(priv)
  PRINT "chain="; TOHEX(chain)
  IF LEFT(TOHEX(re), 66) = TOHEX(pub) THEN
    PRINT "match=SI"
  ELSE
    PRINT "match=NO"
  END IF

  DIM sd(63) AS BYTE
  MnToSeed("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", "TREZOR", sd)
  PRINT "seed="; TOHEX(sd)

  DIM msg() AS BYTE = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
  DIM sig(79) AS BYTE
  Sign(msg, priv, sig)

  DIM h() AS BYTE = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000")
  Diff(h)
END SUB
