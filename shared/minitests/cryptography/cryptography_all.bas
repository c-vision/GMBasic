' MiniTest NAMESPACE CRYPTOGRAPHY — tutti i 40 simboli della superficie.
'
' Superficie (shared/spec/registry/surface.csv): AESDECRYPT, AESENCRYPT,
' CHACHA20DECRYPT, CHACHA20ENCRYPT, CHACHA20POLY1305DECRYPT, CHACHA20POLY1305ENCRYPT,
' DECRYPTASYM, ENCRYPTASYM, FROMBASE64, FROMBASE64URL, FROMHEX, HASHFILE, HMACSHA256,
' HMACSHA384, HMACSHA512, KEYEXPORT, KEYGEN, KEYIMPORT, MD5, PASSWORDHASH,
' PASSWORDVERIFY, RANDOMBYTES, RANDOMCRYPTOBYTES, RANDOMDOUBLE, RANDOMINT,
' RANDOMNORMAL, SHA1, SHA256, SHA512, SIGN, STRHASHFILE, STRMD5, STRSHA1, STRSHA256,
' STRSHA512, TOBASE64, TOBASE64URL, TOHEX, VERIFY, VERIFYRAW.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Gli hash sono confrontati in forma esadecimale (TOHEX) per
' compattezza; le funzioni random verificano solo i FATTI (range/lunghezza).
' EXPECT: md5=5d41402abc4b2a76b9719d911017c592|sha1=aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d|sha256=2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824|sha512=9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca72323c3d99ba5c11d7c7acc6e14b8c5da0c4663475c2e5c3adef46f73bcdec043|strmd5=5d41402abc4b2a76b9719d911017c592|strsha1=aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d|strsha256=2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824|strsha512=9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca72323c3d99ba5c11d7c7acc6e14b8c5da0c4663475c2e5c3adef46f73bcdec043|hmac256=5031fe3d989c6d1537a013fa6e739da23463fdaec3b70137d828e36ace221bd0|hmac384=c5f97ad9fd1020c174d7dc02cf83c4c1bf15ee20ec555b690ad58e62da8a00ee44ccdb65cb8c80acfd127ebee568958a|hmac512=3c5953a18f7303ec653ba170ae334fafa08e3846f2efe317b87efce82376253cb52a8c31ddcde5a3a2eee183c2b34cb91f85e64ddbc325f7692b199473579c58|b64=aGVsbG8=|fromb64=68656c6c6f|b64url=aGVsbG8_|fromb64url=68656c6c6f3f|hex=4142|fromhex=4142|aes= 11 |aes0= 104|chacha= 5 |chacha0= 104|polyC=[74, 42, 61, 159, 47]|polyT=[250, 219, 7, 70, 81, 71, 79, 1, 164, 156, 181, 190, 118, 48, 33, 68]|polyD=[104, 101, 108, 108, 111]|verify=-1 |tamper= 0|export=-1|import=-1|asym= 5 |asym0= 104|pwd=-1 |pwdwrong= 0|rndi=-1|rndd=-1|rndn=-1|rndb=-1 |rndc=-1|strhashfile=2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824|hashfile=2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824|vraw=-1 |vrawbad= 0
IMPORT CRYPTOGRAPHY
IMPORT IO

SUB Main()
  ' ── hash e string hash ───────────────────────────────────────────────────
  PRINT "md5="; TOHEX(MD5("hello"))
  PRINT "sha1="; TOHEX(SHA1("hello"))
  PRINT "sha256="; TOHEX(SHA256("hello"))
  PRINT "sha512="; TOHEX(SHA512("hello"))
  PRINT "strmd5="; STRMD5("hello")
  PRINT "strsha1="; STRSHA1("hello")
  PRINT "strsha256="; STRSHA256("hello")
  PRINT "strsha512="; STRSHA512("hello")

  ' ── HMAC ─────────────────────────────────────────────────────────────────
  PRINT "hmac256="; HMACSHA256("data", "key")
  PRINT "hmac384="; HMACSHA384("data", "key")
  PRINT "hmac512="; HMACSHA512("data", "key")

  ' ── basi e codifiche ─────────────────────────────────────────────────────
  PRINT "b64="; TOBASE64("hello"); "|fromb64="; TOHEX(FROMBASE64(TOBASE64("hello")))
  PRINT "b64url="; TOBASE64URL("hello?"); "|fromb64url="; TOHEX(FROMBASE64URL(TOBASE64URL("hello?")))
  PRINT "hex="; TOHEX("AB"); "|fromhex="; TOHEX(FROMHEX("4142"))

  ' ── simmetrico ───────────────────────────────────────────────────────────
  DIM aes() AS BYTE = AESENCRYPT("hello world", "0123456789abcdef")
  DIM adec() AS BYTE = AESDECRYPT(aes, "0123456789abcdef")
  PRINT "aes="; LEN(adec); "|aes0="; adec(0)

  DIM key() AS BYTE = FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
  DIM nonce() AS BYTE = FROMHEX("000000000000004a00000000")
  DIM cenc() AS BYTE = CHACHA20ENCRYPT("hello", key, nonce)
  DIM cdec() AS BYTE = CHACHA20DECRYPT(cenc, key, nonce)
  PRINT "chacha="; LEN(cdec); "|chacha0="; cdec(0)

  DIM aad() AS BYTE = FROMHEX("01020304")
  DIM poly = CHACHA20POLY1305ENCRYPT("hello", key, nonce, aad)
  PRINT "polyC="; poly("ciphertext")
  PRINT "polyT="; poly("tag")
  PRINT "polyD="; CHACHA20POLY1305DECRYPT(poly("ciphertext"), poly("tag"), key, nonce, aad)

  ' ── asimmetrico ──────────────────────────────────────────────────────────
  DIM keys = KEYGEN("ECDSA", 256)
  DIM sig = SIGN("hello", keys(0))
  PRINT "verify="; VERIFY(sig, "hello", keys(1)); "|tamper="; VERIFY(sig, "x", keys(1))
  DIM pem AS STRING
  pem = KEYEXPORT(keys(0))
  PRINT "export="; (LEN(pem) > 0)
  DIM imp = KEYIMPORT(pem, 0)
  DIM sig2 = SIGN("hello", imp)
  PRINT "import="; VERIFY(sig2, "hello", keys(1))

  DIM rk = KEYGEN("RSA", 2048)
  DIM e() AS BYTE = ENCRYPTASYM("hello", rk(1))
  DIM dd() AS BYTE = DECRYPTASYM(e, rk(0))
  PRINT "asym="; LEN(dd); "|asym0="; dd(0)

  ' ── password ─────────────────────────────────────────────────────────────
  DIM ph AS STRING
  ph = PASSWORDHASH("secret")
  PRINT "pwd="; PASSWORDVERIFY("secret", ph); "|pwdwrong="; PASSWORDVERIFY("wrong", ph)

  ' ── random: solo fatti (valori casuali) ──────────────────────────────────
  DIM ri AS INTEGER
  ri = RANDOMINT(1, 10)
  PRINT "rndi="; (ri >= 1 AND ri <= 10)
  DIM rd AS DOUBLE
  rd = RANDOMDOUBLE(0, 1)
  PRINT "rndd="; (rd >= 0 AND rd <= 1)
  DIM rn AS DOUBLE
  rn = RANDOMNORMAL(0, 1)
  PRINT "rndn="; (ABS(rn) < 100)
  PRINT "rndb="; (LEN(TOHEX(RANDOMBYTES(8))) = 16); "|rndc="; (LEN(TOHEX(RANDOMCRYPTOBYTES(8))) = 16)

  ' ── hash su file ─────────────────────────────────────────────────────────
  DIM path AS STRING
  path = "/tmp/gmb_mt_crypto_all.txt"
  DIM fh AS LONG
  fh = IO.FCREATE(path)
  IO.FWRITE(fh, "hello")
  IO.FCLOSE(fh)
  PRINT "strhashfile="; STRHASHFILE(path, "SHA256")
  PRINT "hashfile="; TOHEX(HASHFILE(path, "SHA256"))
  KILL path

  ' ── VERIFYRAW: vettore RFC 8032 (Ed25519) ────────────────────────────────
  DIM pub AS STRING
  pub = "PUAXw+hDiVqStwqnTRt+vJyYLM8uxJaMwM1V8Sr0Zgw="
  DIM vraw() AS BYTE = FROMHEX("92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00")
  DIM msg() AS BYTE = FROMHEX("72")
  DIM bad() AS BYTE = FROMHEX("73")
  PRINT "vraw="; VERIFYRAW(vraw, msg, pub); "|vrawbad="; VERIFYRAW(vraw, bad, pub)
END SUB
