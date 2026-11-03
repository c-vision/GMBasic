' MiniTest NAMESPACE BLOCKCHAIN — tutte le 24 funzioni della superficie.
' Unione dei MiniTest per-funzione (stessi input e vettori noti): ABI, Base32/58,
' hash (Keccak/BLAKE2b/RIPEMD160 str+bytes), varint, RLP, BIP39, secp256k1, EIP-55, CONTRACT.
' EXPECT: k=1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8|r=108f07b8382412612c048d07d13f814118445acd|b32=NBSWY3DP|b58=Cn8eVZg|b32d=68656c6c6f|b58d=68656c6c6f|k=1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8|b=324dcf027dd4a30a932c441f365a25e86b173defa4b8e58948253471b81b72cf|r=108f07b8382412612c048d07d13f814118445acd|e0=00|e1=01|e127=7f|d127= 127|rt= 127|e150=9601|e300=ac02|d150= 150|d300= 300|rlpA=41|rlpDog=83646f67|rlpEmpty=80|sel=0x70a08231|enc=0x70a082310000000000000000000000000000000000000000000000000000000000000001|dec= 1|priv=cd99dfddcc661bd850c11c510a05e52e2327aa662bc97772ba0e46c2e074f5db|privlen= 32|chain=76b421d62999fb62022cc8f01200bb31a169daf72673eba7e58442b8b67cbfc9|match=SI|diff= 5|seed=c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04|words= 12|eth=0x7e5f4552091a69125d5dfcb7b8c2659029395bdf|cks=0x7E5F4552091A69125d5DfCb7b8C2659029395Bdf|cs=0x5AaEb6053f3e94c096b9A57e4e0A27D1c5f6D|btcMain=1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH|btcTest=mrCDrCybB6J1vRfbwM5hemdJz73FwDBC8r|sigprefix=30|sig2prefix=30|sig2len>=68=-1|addr=0x1234567890123456789012345678901234567890|iface=IERC20
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB T0_encodings()
DIM s$ = "hello"
  PRINT "k="; STRKECCAK256(s$)
  PRINT "r="; STRRIPEMD160(s$)
  PRINT "b32="; BASE32ENCODE(s$)
  PRINT "b58="; BASE58ENCODE(s$)
END SUB

SUB T1_decodings()
PRINT "b32d="; TOHEX(BASE32DECODE("NBSWY3DP"))
  PRINT "b58d="; TOHEX(BASE58DECODE("Cn8eVZg"))
END SUB

SUB T2_hashbytes()
PRINT "k="; TOHEX(KECCAK256("hello"))
  PRINT "b="; TOHEX(BLAKE2B256("hello"))
  PRINT "r="; TOHEX(RIPEMD160("hello"))
END SUB

SUB T3_varint()
PRINT "e0="; TOHEX(VARINTENCODE(0))
  PRINT "e1="; TOHEX(VARINTENCODE(1))
  PRINT "e127="; TOHEX(VARINTENCODE(127))
  DIM b() = FROMHEX("7f")
  PRINT "d127="; VARINTDECODE(b())
  PRINT "rt="; VARINTDECODE(VARINTENCODE(127))
END SUB

SUB T4_varint_big()
PRINT "e150="; TOHEX(VARINTENCODE(150))
  PRINT "e300="; TOHEX(VARINTENCODE(300))
  DIM a() = FROMHEX("9601")
  DIM c() = FROMHEX("ac02")
  PRINT "d150="; VARINTDECODE(a())
  PRINT "d300="; VARINTDECODE(c())
END SUB

SUB T5_rlp_abienc()
PRINT "rlpA="; TOHEX(RLPENCODE("A"))
  PRINT "rlpDog="; TOHEX(RLPENCODE("dog"))
  PRINT "rlpEmpty="; TOHEX(RLPENCODE(""))
  PRINT "sel="; ABIENCODE("balanceOf(address)")
END SUB

SUB T6_abiargs()
PRINT "enc="; ABIENCODE("balanceOf(address)", 1)
  DIM w$ = "0x0000000000000000000000000000000000000000000000000000000000000001"
  DIM r = ABIDECODE(w$, {"uint256"})
  PRINT "dec="; r(0)
END SUB

SUB T7_derivekey()
DIM seed() = FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
  DIM dk = DERIVEKEY(seed(), "m/44'/60'/0'/0/0")
  DIM priv() = dk("privateKey")
  DIM chain() = dk("chainCode")
  DIM pub() = dk("publicKey")
  PRINT "priv="; TOHEX(priv())
  PRINT "privlen="; LEN(priv())
  PRINT "chain="; TOHEX(chain())
  DIM re() = GETPUBLICKEY(priv())
  IF TOHEX(re()) = TOHEX(pub()) THEN
    PRINT "match=SI"
  ELSE
    PRINT "match=NO"
  END IF
END SUB

SUB T8_difficulty()
DIM h() = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000")
  PRINT "diff="; BLOCKDIFFICULTY(h(), 553648127)
END SUB

SUB T9_mnemonic()
DIM seed() = MNEMONICTOSEED("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", "TREZOR")
  PRINT "seed="; TOHEX(seed())
  DIM m$ = GENERATEMNEMONIC(12)
  DIM w = 1
  FOR i = 1 TO LEN(m$)
    IF MID(m$, i, 1) = " " THEN w = w + 1
  NEXT i
  PRINT "words="; w
END SUB

SUB T10_ethaddrpub()
DIM pub() = FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8")
  DIM a$ = ETHADDRFROMPUB(pub())
  PRINT "eth="; a$
  PRINT "cks="; ETHADDRCHECKSUM(a$)
END SUB

SUB T11_addrs_ok()
PRINT "cs="; ETHADDRCHECKSUM("0x5aaeb6053f3e94c096b9a57e4e0a27d1c5f6d")
  DIM pub() = FROMHEX("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798")
  PRINT "btcMain="; BTCADDRFROMPUB(pub(), FALSE)
  PRINT "btcTest="; BTCADDRFROMPUB(pub(), TRUE)
END SUB

SUB T12_sign_contract()
DIM msg() = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
  DIM priv() = FROMHEX("0000000000000000000000000000000000000000000000000000000000000001")
  DIM sig() = SECP256K1SIGN(msg(), priv())
  PRINT "sigprefix="; LEFT(TOHEX(sig()), 2)
  ' Chiave privata ESTRATTA da un dict boxed (bytes, non lista): caso d'uso del Sample
  ' Crypto/PortafoglioHD (DERIVEKEY → "privateKey").
  DIM dk = DERIVEKEY(FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"), "m/44'/60'/0'/0/0")
  DIM dpriv() = dk("privateKey")
  DIM sig2() = SECP256K1SIGN(msg(), dpriv())
  PRINT "sig2prefix="; LEFT(TOHEX(sig2()), 2)
  PRINT "sig2len>=68="; (LEN(sig2()) >= 68)
  DIM c = CONTRACT("0x1234567890123456789012345678901234567890", "IERC20")
  PRINT "addr="; c("address")
  PRINT "iface="; c("interface")
END SUB

SUB Main()
  T0_encodings()
  T1_decodings()
  T2_hashbytes()
  T3_varint()
  T4_varint_big()
  T5_rlp_abienc()
  T6_abiargs()
  T7_derivekey()
  T8_difficulty()
  T9_mnemonic()
  T10_ethaddrpub()
  T11_addrs_ok()
  T12_sign_contract()
END SUB
