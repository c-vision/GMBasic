' ============================================================================
' CRYPTOGRAPHY — cifratura simmetrica AES, ChaCha20 e ChaCha20-Poly1305 (AEAD).
' Esempio per il namespace CRYPTOGRAPHY (pacchetto `crypto`).
' NB (2026-11-13): API del pacchetto FFI — ogni operazione scrive l'output in un array BYTE
' `out()` e ritorna la LUNGHEZZA (n), quindi non esistono più i valori di ritorno "array".
' ============================================================================
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY AS CR

SUB Main()
  ' --- AES-GCM con password: cifra -> byte[], decifra -> byte[]
  DIM enc(256) AS BYTE
  DIM nEnc AS LONG = CR.AESENCRYPT("hello world", "0123456789abcdef", enc)
  DIM dec(256) AS BYTE
  DIM nDec AS LONG = CR.AESDECRYPT(enc, nEnc, "0123456789abcdef", dec)
  PRINT "aes:     "; LEFT(CR.BYTESHEX(dec), nDec * 2); " ("; nDec; " byte)"

  ' --- ChaCha20 (key 32 byte + nonce 12 byte come array BYTE)
  DIM key(31) AS BYTE   ' 32 byte ESATTI (DIM n => n+1 elementi: la key ChaCha20 e' 32)
  DIM nKey AS LONG = CR.FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", key)
  DIM nonce(11) AS BYTE ' 12 byte ESATTI (nonce ChaCha20)
  DIM nNonce AS LONG = CR.FROMHEX("000000000000004a00000000", nonce)
  DIM cenc(256) AS BYTE
  DIM nCEnc AS LONG = CR.CHACHA20ENCRYPT("hello", key, nonce, cenc)
  DIM cdec(256) AS BYTE
  DIM nCDec AS LONG = CR.CHACHA20DECRYPT(cenc, nCEnc, key, nonce, cdec)
  PRINT "chacha20:"; LEFT(CR.BYTESHEX(cdec), nCDec * 2); " ("; nCDec; " byte)"

  ' --- ChaCha20-Poly1305 (AEAD): la AAD è una STRINGA, il ciphertext va in out()
  DIM ct(256) AS BYTE
  DIM nCt AS LONG = CR.CHACHA20POLY1305ENCRYPT("hello", key, nonce, "01020304", ct)
  PRINT "ct:      "; LEFT(CR.BYTESHEX(ct), nCt * 2); " ("; nCt; " byte)"
  DIM pt(256) AS BYTE
  DIM nPt AS LONG = CR.CHACHA20POLY1305DECRYPT(ct, nCt, key, nonce, "01020304", pt)
  PRINT "aead:    "; LEFT(CR.BYTESHEX(pt), nPt * 2); " ("; nPt; " byte)"
END SUB
