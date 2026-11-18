' MiniTest NAMESPACE BLOCKCHAIN — tutte le funzioni del pacchetto `blockchain` (ABI a BYTE).
' Unione dei MiniTest per-funzione (stessi input e vettori noti).
' EXPECT: k=1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8|b=324dcf027dd4a30a932c441f365a25e86b173defa4b8e58948253471b81b72cf|r=108f07b8382412612c048d07d13f814118445acd|b32=NBSWY3DP|b58=Cn8eVZg|b32d=68656c6c6f|b58d=68656c6c6f|e0=00|e300=ac02|d300= 300|rlpDog=83646f67|sel=0x70a08231|enc=0x70a082310000000000000000000000000000000000000000000000000000000000000001|dec= 1|priv=cd99dfddcc661bd850c11c510a05e52e2327aa662bc97772ba0e46c2e074f5db|match=SI|diff= 5|seed=c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04|words= 12|eth=0x7e5f4552091a69125d5dfcb7b8c2659029395bdf|cks=0x7E5F4552091A69125d5DfCb7b8C2659029395Bdf|cs=0x5AaEb6053f3e94c096b9A57e4e0A27D1c5f6D|btcMain=1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH|btcTest=mrCDrCybB6J1vRfbwM5hemdJz73FwDBC8r|sigprefix=30|derlen>=68=-1
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB T0_hash()
  DIM d(4) AS BYTE
  DIM _n_d AS LONG
  _n_d = FROMHEX("68656c6c6f", d)
  DIM k(31) AS BYTE
  KECCAK256(d, k)
  PRINT "k="; BYTESHEX(k)
  DIM b(31) AS BYTE
  BLAKE2B256(d, b)
  PRINT "b="; BYTESHEX(b)
  DIM r(19) AS BYTE
  RIPEMD160(d, r)
  PRINT "r="; BYTESHEX(r)
END SUB

SUB T1_enc()
  DIM d(4) AS BYTE
  DIM _n_d AS LONG
  _n_d = FROMHEX("68656c6c6f", d)
  PRINT "b32="; BASE32ENCODE(d)
  PRINT "b58="; BASE58ENCODE(d)
  DIM o(63) AS BYTE
  DIM n AS LONG
  n = BASE32DECODE("NBSWY3DP", o)
  PRINT "b32d="; LEFT(BYTESHEX(o), n * 2)
  n = BASE58DECODE("Cn8eVZg", o)
  PRINT "b58d="; LEFT(BYTESHEX(o), n * 2)
END SUB

SUB T2_varint()
  DIM o(9) AS BYTE
  DIM n AS LONG
  n = VARINTENCODE(0, o)
  PRINT "e0="; LEFT(BYTESHEX(o), n * 2)
  n = VARINTENCODE(300, o)
  PRINT "e300="; LEFT(BYTESHEX(o), n * 2)
  DIM c(1) AS BYTE
  DIM _n_c AS LONG
  _n_c = FROMHEX("ac02", c)
  PRINT "d300="; VARINTDECODE(c)
END SUB

SUB T3_rlp_abi()
  DIM o(63) AS BYTE
  DIM n AS LONG
  DIM dog(2) AS BYTE
  DIM _n_dog AS LONG
  _n_dog = FROMHEX("646f67", dog)
  n = RLPENCODE(dog, o)
  PRINT "rlpDog="; LEFT(BYTESHEX(o), n * 2)
  PRINT "sel="; ABISELECTOR("balanceOf(address)")
  DIM nums(0) AS DOUBLE
  nums(0) = 1
  PRINT "enc="; ABIENCODE("balanceOf(address)", nums)
  DIM w$ AS STRING = "0x0000000000000000000000000000000000000000000000000000000000000001"
  DIM out(9) AS DOUBLE
  n = ABIDECODE(w$, out)
  PRINT "dec="; out(0)
END SUB

SUB T4_derive()
  DIM seed(31) AS BYTE
  DIM _n_seed AS LONG
  _n_seed = FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", seed)
  DIM dk(96) AS BYTE
  DIM n AS LONG
  n = DERIVEKEY(seed, "m/44'/60'/0'/0/0", dk)
  DIM priv(31) AS BYTE
  DIM pub(32) AS BYTE
  DIM i AS LONG
  FOR i = 0 TO 31
    priv(i) = dk(i)
  NEXT i
  FOR i = 0 TO 32
    pub(i) = dk(32 + i)
  NEXT i
  PRINT "priv="; BYTESHEX(priv)
  DIM re(63) AS BYTE
  n = GETPUBLICKEY(priv, re)
  IF LEFT(BYTESHEX(re), n * 2) = BYTESHEX(pub) THEN
    PRINT "match=SI"
  ELSE
    PRINT "match=NO"
  END IF
END SUB

SUB T5_difficulty()
  DIM h(31) AS BYTE
  DIM _n_h AS LONG
  _n_h = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000", h)
  PRINT "diff="; BLOCKDIFFICULTY(h, 553648127)
END SUB

SUB T6_mnemonic()
  DIM seed(63) AS BYTE
  DIM n AS LONG
  n = MNEMONICTOSEED("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", "TREZOR", seed)
  PRINT "seed="; BYTESHEX(seed)
  DIM m$ AS STRING
  m$ = GENERATEMNEMONIC(12)
  DIM w AS LONG
  w = 1
  DIM i AS LONG
  FOR i = 1 TO LEN(m$)
    IF MID(m$, i, 1) = " " THEN w = w + 1
  NEXT i
  PRINT "words="; w
END SUB

SUB T7_addresses()
  DIM pub(64) AS BYTE
  DIM _n_pub AS LONG
  _n_pub = FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8", pub)
  DIM a$ AS STRING
  a$ = ETHADDRFROMPUB(pub)
  PRINT "eth="; a$
  PRINT "cks="; ETHADDRCHECKSUM(a$)
  PRINT "cs="; ETHADDRCHECKSUM("0x5aaeb6053f3e94c096b9a57e4e0a27d1c5f6d")
  DIM cpub(32) AS BYTE
  DIM _n_cpub AS LONG
  _n_cpub = FROMHEX("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798", cpub)
  PRINT "btcMain="; BTCADDRFROMPUB(cpub, FALSE)
  PRINT "btcTest="; BTCADDRFROMPUB(cpub, TRUE)
END SUB

SUB T8_sign()
  DIM msg(31) AS BYTE
  DIM _n_msg AS LONG
  _n_msg = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", msg)
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM sig(79) AS BYTE
  DIM n AS LONG
  n = SECP256K1SIGN(msg, priv, sig)
  PRINT "sigprefix="; LEFT(BYTESHEX(sig), 2)
  PRINT "derlen>=68="; (n >= 68)
END SUB

SUB Main()
  T0_hash()
  T1_enc()
  T2_varint()
  T3_rlp_abi()
  T4_derive()
  T5_difficulty()
  T6_mnemonic()
  T7_addresses()
  T8_sign()
END SUB
