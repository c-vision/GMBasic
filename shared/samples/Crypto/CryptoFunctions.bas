' FILE: Crypto/CryptoFunctions.bas
' =================================================================================================
'       GUIDA TEORICA: CRITTOGRAFIA, HASHING E GESTIONE CHIAVI
' =================================================================================================
' Copre il namespace `CRYPTOGRAPHY` (pacchetto FFI `crypto`).
'
' NB (2026-11-13) — API del PACCHETTO (dopo la migrazione da builtin):
'   • le operazioni su BYTE scrivono in un array `out() AS BYTE` e ritornano la LUNGHEZZA (n);
'     non esistono più valori di ritorno "array";
'   • i decodificatori sono `FROMHEX(testo, out())`, `FROMBASE64(testo, out())`;
'   • l'esadecimale di un ARRAY si rende con `BYTESHEX(a())` (la FUNCTION `TOHEX` prende STRINGA);
'   • `KEYGEN(algo, bits, h())` riempie `h(0)`=privata, `h(1)`=pubblica; `SIGN`/`ENCRYPTASYM`
'     scrivono in `out()` e ritornano n, che `VERIFY`/`DECRYPTASYM` richiedono;
'   • `KEYIMPORT(pem)` RITORNA l'handle.
' Per questo il sample usa `REQUIRE "crypto"` prima di `IMPORT CRYPTOGRAPHY`.
' =================================================================================================
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB Main()
    DIM testo$ AS STRING
    testo$ = "Gm-Basic"
    ' I byte del testo, per le primitive che lavorano su array
    DIM testoBytes(31) AS BYTE
    DIM nTesto AS LONG = FROMHEX("474d2d4241534943", testoBytes)   ' "GM-BASIC"

    ' --- 1. Test di Hashing ---
    PRINT "--- 1. Test di Hashing ---"
    PRINT "Testo Originale: "; testo$
    ' Le funzioni prefissate STR* ritornano direttamente la stringa esadecimale
    PRINT "STRMD5(testo$):       "; STRMD5(testo$)
    PRINT "STRSHA1(testo$):      "; STRSHA1(testo$)
    PRINT "STRSHA256(testo$):    "; STRSHA256(testo$)
    PRINT "STRSHA512(testo$):    "; STRSHA512(testo$)
    ' HASH(testo, algo, out()) scrive i BYTE del digest: si rendono con BYTESHEX
    DIM dig(63) AS BYTE
    DIM nDig AS LONG = HASH(testo$, "MD5", dig)
    PRINT "MD5(bytes):     "; LEFT(BYTESHEX(dig), nDig * 2)
    nDig = HASH(testo$, "SHA256", dig)
    PRINT "SHA256(bytes):  "; LEFT(BYTESHEX(dig), nDig * 2)
    PRINT

    ' --- 2. Test di Crittografia Simmetrica (AES-GCM con password) ---
    PRINT "--- 2. Test di Crittografia Simmetrica (AES) ---"
    DIM password$ AS STRING
    password$ = "una password molto sicura!"
    DIM cifrato(255) AS BYTE
    DIM nCif AS LONG = AESENCRYPT(testo$, password$, cifrato)
    PRINT "Dati originali (HEX): "; TOHEX(testo$)
    PRINT "Dati cifrati (HEX):   "; LEFT(BYTESHEX(cifrato), nCif * 2)
    DIM chiaro(255) AS BYTE
    DIM nChiaro AS LONG = AESDECRYPT(cifrato, nCif, password$, chiaro)
    PRINT "Dati decrittati (HEX):"; LEFT(BYTESHEX(chiaro), nChiaro * 2)
    IF LEFT(BYTESHEX(chiaro), nChiaro * 2) = TOHEX(testo$) THEN PRINT "Risultato AES: OK!" ELSE PRINT "Risultato AES: ERRORE!"
    PRINT

    ' --- 3. Test di Firma Digitale e Crittografia Asimmetrica ---
    PRINT "--- 3. Test di Firma Digitale (ECDSA) e Crittografia (RSA) ---"
    ' Chiavi per la firma: h(0)=privata, h(1)=pubblica
    DIM ecdsa(2) AS LONG
    DIM nK AS LONG = KEYGEN("ECDSA", 256, ecdsa)
    PRINT "Coppia di chiavi ECDSA generata (handle privato "; ecdsa(0); ", pubblico "; ecdsa(1); ")"

    DIM messaggio$ AS STRING
    messaggio$ = "Questo e' un messaggio da firmare."
    DIM firma(255) AS BYTE
    DIM nFirma AS LONG = SIGN(messaggio$, ecdsa(0), firma)
    PRINT "Firma (HEX): "; LEFT(BYTESHEX(firma), nFirma * 2)
    IF VERIFY(firma, nFirma, messaggio$, ecdsa(1)) THEN PRINT "Verifica Firma: OK!" ELSE PRINT "Verifica Firma: ERRORE!"

    ' RSA 2048: cifratura con la chiave PUBBLICA, decifratura con la PRIVATA
    DIM rsa(2) AS LONG
    DIM nKRsa AS LONG = KEYGEN("RSA", 2048, rsa)
    DIM encRsa(511) AS BYTE
    DIM nEncRsa AS LONG = ENCRYPTASYM("Top Secret", rsa(1), encRsa)
    DIM decRsa(511) AS BYTE
    DIM nDecRsa AS LONG = DECRYPTASYM(encRsa, nEncRsa, rsa(0), decRsa)
    IF LEFT(BYTESHEX(decRsa), nDecRsa * 2) = TOHEX("Top Secret") THEN PRINT "Risultato RSA: OK!" ELSE PRINT "Risultato RSA: ERRORE!"

    ' Export/Import della chiave pubblica: KEYIMPORT ritorna il nuovo handle
    DIM pem$ AS STRING
    pem$ = KEYEXPORT(ecdsa(1))
    DIM hImport AS LONG = KEYIMPORT(pem$)
    PRINT "Chiave pubblica esportata e re-importata (handle "; hImport; ")"
    IF VERIFY(firma, nFirma, messaggio$, hImport) THEN PRINT "Verifica con Chiave Importata: OK!" ELSE PRINT "Verifica con Chiave Importata: ERRORE!"

    ' Pulizia risorse (gli handle non sono garbage-collected)
    DIM nC1 AS LONG = KEYCLOSE(ecdsa(0))
    DIM nC2 AS LONG = KEYCLOSE(ecdsa(1))
    DIM nC3 AS LONG = KEYCLOSE(hImport)
    DIM nC4 AS LONG = KEYCLOSE(rsa(0))
    DIM nC5 AS LONG = KEYCLOSE(rsa(1))
    PRINT

    ' --- 4. Test Utility e Funzioni Random ---
    PRINT "--- 4. Test Utility e Funzioni Random ---"
    PRINT "TOBASE64('abc'): "; TOBASE64("abc")
    DIM b64(31) AS BYTE
    DIM nB64 AS LONG = FROMBASE64("YWJj", b64)
    PRINT "FROMBASE64('YWJj'): "; LEFT(BYTESHEX(b64), nB64 * 2); " (hex dei byte)"
    PRINT "TOHEX('abc'):   "; TOHEX("abc")
    DIM daHex(31) AS BYTE
    DIM nHex AS LONG = FROMHEX("616263", daHex)
    PRINT "FROMHEX('616263'): "; CHR$(daHex(0)); CHR$(daHex(1)); CHR$(daHex(2)); " ("; nHex; " byte)"
    ' Numeri casuali: si verificano le PROPRIETA' (i valori cambiano a ogni run)
    DIM rnd(7) AS BYTE
    DIM nRnd AS LONG = RANDOMCRYPTOBYTES(4, rnd)
    PRINT "RANDOMCRYPTOBYTES(4): "; LEN(LEFT(BYTESHEX(rnd), nRnd * 2)); " caratteri hex (attesi 8)"
    DIM ri AS LONG
    ri = RANDOMINT(1, 10)
    IF ri >= 1 AND ri <= 10 THEN PRINT "RANDOMINT(1, 10): ok" ELSE PRINT "RANDOMINT(1, 10): ERRORE"
    DIM rd AS DOUBLE
    rd = RANDOMDOUBLE(1.0, 5.0)
    IF rd >= 1.0 AND rd <= 5.0 THEN PRINT "RANDOMDOUBLE(1,5): ok" ELSE PRINT "RANDOMDOUBLE(1,5): ERRORE"
    PRINT

    ' --- 5. Test di Crittografia a Flusso (ChaCha20 / ChaCha20-Poly1305) ---
    PRINT "--- 5. Test di Crittografia a Flusso (ChaCha20) ---"
    DIM messaggio_chacha$ AS STRING
    messaggio_chacha$ = "Questo e' un messaggio segreto per la cifratura a flusso."
    ' ChaCha20: chiave 32 byte, nonce 12 byte (DIM n => n+1 elementi)
    DIM chacha_key(31) AS BYTE
    DIM nKey AS LONG = RANDOMCRYPTOBYTES(32, chacha_key)
    DIM chacha_nonce(11) AS BYTE
    DIM nNonce AS LONG = RANDOMCRYPTOBYTES(12, chacha_nonce)
    PRINT "Messaggio originale: '"; messaggio_chacha$; "'"

    ' -- 5a. ChaCha20 semplice --
    PRINT "Cifratura/Decifratura con ChaCha20 semplice..."
    DIM cifrato_chacha(511) AS BYTE
    DIM nCifC AS LONG = CHACHA20ENCRYPT(messaggio_chacha$, chacha_key, chacha_nonce, cifrato_chacha)
    DIM decrittato_chacha(511) AS BYTE
    DIM nDecC AS LONG = CHACHA20DECRYPT(cifrato_chacha, nCifC, chacha_key, chacha_nonce, decrittato_chacha)
    IF LEFT(BYTESHEX(decrittato_chacha), nDecC * 2) = TOHEX(messaggio_chacha$) THEN PRINT "Risultato ChaCha20 Semplice: OK!" ELSE PRINT "Risultato ChaCha20 Semplice: ERRORE!"

    ' -- 5b. ChaCha20-Poly1305 (AEAD): out = ciphertext || tag(16). AAD = STRINGA --
    PRINT "Cifratura/Decifratura con ChaCha20-Poly1305 (AEAD)..."
    DIM aead(511) AS BYTE
    DIM nAead AS LONG = CHACHA20POLY1305ENCRYPT(messaggio_chacha$, chacha_key, chacha_nonce, "010203", aead)
    PRINT "Testo cifrato AEAD (HEX): "; LEFT(BYTESHEX(aead), nAead * 2)
    DIM aeadPt(511) AS BYTE
    DIM nAeadPt AS LONG = CHACHA20POLY1305DECRYPT(aead, nAead, chacha_key, chacha_nonce, "010203", aeadPt)
    IF LEFT(BYTESHEX(aeadPt), nAeadPt * 2) = TOHEX(messaggio_chacha$) THEN PRINT "Risultato ChaCha20-Poly1305: OK!" ELSE PRINT "Risultato ChaCha20-Poly1305: ERRORE!"

    PRINT "--- TEST COMPLETATO ---"
END SUB
