' ============================================================================
' CRYPTOGRAPHY — cifratura simmetrica AES, ChaCha20 e ChaCha20-Poly1305 (AEAD).
' Esempio per il namespace CRYPTOGRAPHY.
' ============================================================================
IMPORT CRYPTOGRAPHY AS CR

SUB Main()
  ' --- AES (key 16 byte come stringa): encrypt → byte[], decrypt → byte[]
  DIM enc() AS BYTE = CR.AESENCRYPT("hello world", "0123456789abcdef")
  DIM dec() AS BYTE = CR.AESDECRYPT(enc, "0123456789abcdef")
  PRINT "aes:     "; dec

  ' --- ChaCha20 (key 32 byte + nonce 12 byte come byte[])
  DIM key() AS BYTE = CR.FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
  DIM nonce() AS BYTE = CR.FROMHEX("000000000000004a00000000")
  DIM cenc() AS BYTE = CR.CHACHA20ENCRYPT("hello", key, nonce)
  DIM cdec() AS BYTE = CR.CHACHA20DECRYPT(cenc, key, nonce)
  PRINT "chacha20:"; cdec

  ' --- ChaCha20-Poly1305 (AEAD): ritorna un dict con ciphertext/tag.
  DIM aad() AS BYTE = CR.FROMHEX("01020304")
  DIM r = CR.CHACHA20POLY1305ENCRYPT("hello", key, nonce, aad)
  PRINT "ct:      "; r("ciphertext")
  PRINT "tag:     "; r("tag")
  PRINT "aead:    "; CR.CHACHA20POLY1305DECRYPT(r("ciphertext"), r("tag"), key, nonce, aad)
END SUB
