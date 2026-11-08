' MiniTest BLOCKCHAIN — copertura ESPLICITA di tutte le 23 funzioni pubbliche del namespace.
' Un solo file che chiama ogni funzione con un input deterministico e verifica i valori
' (firma e mnemonica sono casuali: si controlla solo forma/lunghezza).
' EXPECT: k=1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8|r=108f07b8382412612c048d07d13f814118445acd|b=324dcf027dd4a30a932c441f365a25e86b173defa4b8e58948253471b81b72cf|b58=Cn8eVZg|b32=NBSWY3DP|b58d= 5 :68656c6c6f|b32d= 5 :68656c6c6f|rlp=8568656c6c6f|v=9601|dv= 150|pub= 33|full= 65|eth=0x7e5f4552091a69125d5dfcb7b8c2659029395bdf|cks=0x7E5F4552091A69125d5DfCb7b8C2659029395Bdf|btc=1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH|btct=mrCDrCybB6J1vRfbwM5hemdJz73FwDBC8r|seed=c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04|hd= 97|mn= 12|sig=30|lenok=-1|diff= 5|abi=0x70a082310000000000000000000000000000000000000000000000000000000000000001|sel=0x70a08231|dec= 1|n= 1
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  ' hashing (KECCAK256, RIPEMD160, BLAKE2B256)
  DIM d() AS BYTE = FROMHEX("68656c6c6f")
  DIM k(31) AS BYTE
  DIM r(19) AS BYTE
  DIM b(31) AS BYTE
  KECCAK256(d, k)
  RIPEMD160(d, r)
  BLAKE2B256(d, b)
  PRINT "k="; TOHEX(k)
  PRINT "r="; TOHEX(r)
  PRINT "b="; TOHEX(b)

  ' encoding (BASE58/BASE32 ENCODE+DECODE, RLPENCODE, VARINTENCODE/DECODE)
  DIM o(31) AS BYTE
  DIM n1 AS LONG
  DIM n2 AS LONG
  DIM n3 AS LONG
  DIM n4 AS LONG
  PRINT "b58="; BASE58ENCODE(d)
  PRINT "b32="; BASE32ENCODE(d)
  n1 = BASE58DECODE(BASE58ENCODE(d), o)
  PRINT "b58d="; n1; ":"; LEFT$(TOHEX(o), n1 * 2)
  n2 = BASE32DECODE(BASE32ENCODE(d), o)
  PRINT "b32d="; n2; ":"; LEFT$(TOHEX(o), n2 * 2)
  DIM rlp(31) AS BYTE
  n3 = RLPENCODE(d, rlp)
  PRINT "rlp="; LEFT$(TOHEX(rlp), n3 * 2)
  DIM vi(15) AS BYTE
  n4 = VARINTENCODE(150, vi)
  PRINT "v="; LEFT$(TOHEX(vi), n4 * 2)
  PRINT "dv="; VARINTDECODE(vi)

  ' chiavi/indirizzi (GETPUBLICKEY, GETFULLPUBLICKEY, ETHADDRFROMPUB,
  ' ETHADDRCHECKSUM, BTCADDRFROMPUB)
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM pub(32) AS BYTE
  DIM full(64) AS BYTE
  PRINT "pub="; GETPUBLICKEY(priv, pub)
  PRINT "full="; GETFULLPUBLICKEY(priv, full)
  PRINT "eth="; ETHADDRFROMPUB(full)
  PRINT "cks="; ETHADDRCHECKSUM(ETHADDRFROMPUB(full))
  PRINT "btc="; BTCADDRFROMPUB(pub, 0)
  PRINT "btct="; BTCADDRFROMPUB(pub, -1)

  ' HD wallet (MNEMONICTOSEED, DERIVEKEY, GENERATEMNEMONIC)
  DIM seed(63) AS BYTE
  DIM nseed AS LONG
  nseed = MNEMONICTOSEED("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about", "TREZOR", seed)
  PRINT "seed="; LEFT$(TOHEX(seed), nseed * 2)
  DIM hd(96) AS BYTE
  PRINT "hd="; DERIVEKEY(seed, "", hd)
  DIM m$ AS STRING
  m$ = GENERATEMNEMONIC(12)
  DIM w AS LONG
  w = 1
  DIM i AS LONG
  FOR i = 1 TO LEN(m$)
    IF MID$(m$, i, 1) = " " THEN w = w + 1
  NEXT i
  PRINT "mn="; w

  ' firma (SECP256K1SIGN) — deterministica solo la forma
  DIM hash() AS BYTE = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
  DIM sig(79) AS BYTE
  DIM n AS LONG
  n = SECP256K1SIGN(hash, priv, sig)
  PRINT "sig="; LEFT$(TOHEX(sig), 2)
  PRINT "lenok="; (n >= 68)

  ' difficoltà (BLOCKDIFFICULTY)
  DIM hdr() AS BYTE = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000")
  PRINT "diff="; BLOCKDIFFICULTY(hdr, 553648127)

  ' ABI (ABIENCODE, ABISELECTOR, ABIDECODE)
  DIM nums(0) AS DOUBLE
  nums(0) = 1
  PRINT "abi="; ABIENCODE("balanceOf(address)", nums)
  PRINT "sel="; ABISELECTOR("balanceOf(address)")
  DIM out(9) AS DOUBLE
  DIM nn AS LONG
  nn = ABIDECODE("0x0000000000000000000000000000000000000000000000000000000000000001", out)
  PRINT "dec="; out(0)
  PRINT "n="; nn
END SUB
