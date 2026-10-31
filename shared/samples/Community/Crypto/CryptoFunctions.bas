' FILE: Crypto/CryptoFunctions.bas
' =================================================================================================
'       GUIDA TEORICA: CRITTOGRAFIA, HASHING E GESTIONE CHIAVI
' =================================================================================================
'
' Questo test copre le funzionalità del namespace `CRYPTOGRAPHY`, che fornisce primitive
' sicure per la protezione dei dati, l'autenticazione e la gestione dell'integrità.
'
' 1. HASHING (MD5, SHA1, SHA256, SHA512)
'    L'hashing trasforma dati di lunghezza arbitraria in una stringa di bit di lunghezza fissa
'    (digest). È un processo unidirezionale (non reversibile).
'    - Usi: Verifica integrità file, stoccaggio password, firme digitali.
'    - Varianti: GM-BASIC offre versioni che accettano stringhe (`STRMD5`) e versioni che
'      accettano array di byte (`MD5`).
'
' 2. CRITTOGRAFIA SIMMETRICA (AES)
'    Usa la stessa chiave sia per cifrare che per decifrare.
'    - `AESENCRYPT`/`AESDECRYPT`: Implementano lo standard AES.
'    - Requisiti: Chiave (Key) e Vettore di Inizializzazione (IV) per garantire che lo stesso
'      testo cifrato due volte produca output diversi.
'
' 3. CRITTOGRAFIA ASIMMETRICA (RSA)
'    Usa una coppia di chiavi: Pubblica (per cifrare) e Privata (per decifrare).
'    - `ENCRYPTASYM`: Cifra dati destinati al possessore della chiave privata.
'    - `DECRYPTASYM`: Decifra dati ricevuti usando la propria chiave privata.
'
' 4. FIRMA DIGITALE (ECDSA)
'    Garantisce autenticità e non ripudio.
'    - `SIGN`: Usa la chiave PRIVATA per firmare un messaggio (o il suo hash).
'    - `VERIFY`: Usa la chiave PUBBLICA per verificare che la firma appartenga al proprietario
'      della chiave privata e che il messaggio non sia stato alterato.
'
' 5. GESTIONE CHIAVI (KEYGEN, EXPORT, IMPORT)
'    GM-BASIC astrae la complessità delle chiavi crittografiche tramite "Handle" (numeri interi).
'    - `KEYGEN`: Genera una nuova coppia di chiavi (RSA o ECDSA) e restituisce gli handle.
'    - `KEYEXPORT`: Serializza una chiave in formato stringa (es. PEM) per lo storage.
'    - `KEYIMPORT`: Carica una chiave da una stringa PEM in un nuovo handle.
'    - `KEYCLOSE`: Libera le risorse associate all'handle della chiave (importante per evitare leak).
'
' 6. ENCODING BINARIO
'    Poiché la crittografia lavora su byte, sono essenziali funzioni per convertire dati binari
'    in formati testuali sicuri:
'    - `Base64`: Standard per trasmettere binari su canali di testo.
'    - `Hex`: Rappresentazione esadecimale (utile per debug e hash).
'
' =================================================================================================

IMPORT STRINGS
IMPORT CRYPTOGRAPHY

SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ CryptoFunctions - Test Suite Completa per le Funzionalità Crittografiche ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
    PRINT

    DIM testo$ = "GM-BASIC"
    ' Crea un array di byte dai valori esadecimali ASCII della stringa "GM-BASIC"
    DIM testoBytes() = FROMHEX("474d2d4241534943") 
    DIM hex_string$ = "616263" ' "abc" in hex

    ' --- 1. Test di Hashing (CRYPTOGRAPHY) ---
    PRINT "--- 1. Test di Hashing ---"
    PRINT "Testo Originale: "; testo$
    ' Funzioni prefissate con STR accettano input stringa direttamente
    PRINT "STRMD5(testo$):       "; STRMD5(testo$)
    PRINT "STRSHA1(testo$):      "; STRSHA1(testo$)
    PRINT "STRSHA256(testo$):    "; STRSHA256(testo$)
    PRINT "STRSHA512(testo$):    "; STRSHA512(testo$)
    ' Funzioni standard accettano array di byte e restituiscono array di byte (convertiti qui in Hex per visualizzazione)
    PRINT "MD5(bytes):     "; TOHEX(MD5(testoBytes()))
    PRINT "SHA1(bytes):    "; TOHEX(SHA1(testoBytes()))
    PRINT "SHA256(bytes):  "; TOHEX(SHA256(testoBytes()))
    PRINT "SHA512(bytes):  "; TOHEX(SHA512(testoBytes()))
    PRINT

    ' --- 2. Test di Crittografia Simmetrica (AES - CRYPTOGRAPHY) ---
    PRINT "--- 2. Test di Crittografia Simmetrica (AES) ---"
    ' Dati binari arbitrari da proteggere
    DIM dati_originali() = FROMHEX("DEADBEEFCAFEBABE0102030405060708")
    DIM password$ = "una password molto sicura!"
    ' Initialization Vector (16 bytes per AES)
    DIM iv() = FROMHEX("000102030405060708090A0B0C0D0E0F")
    PRINT "Dati originali (HEX): "; TOHEX(dati_originali())
    
    ' Cifratura
    DIM dati_cifrati() = AESENCRYPT(dati_originali(), password$, iv())
    PRINT "Dati cifrati (Base64): "; TOBASE64(dati_cifrati())
    
    ' Decifratura
    DIM dati_decrittati() = AESDECRYPT(dati_cifrati(), password$, iv())
    PRINT "Dati decrittati (HEX): "; TOHEX(dati_decrittati())
    
    IF TOHEX(dati_originali()) = TOHEX(dati_decrittati()) THEN PRINT "Risultato AES: OK!" ELSE PRINT "Risultato AES: ERRORE!"
    PRINT

    ' --- 3. Test di Firma Digitale e Crittografia Asimmetrica ---
    PRINT "--- 3. Test di Firma Digitale (ECDSA) e Crittografia (RSA) ---"
    
    ' Generazione Chiavi per Firma (Elliptic Curve Digital Signature Algorithm)
    DIM pvtHandle, pubHandle
    KEYGEN "ECDSA", pvtHandle, pubHandle
    PRINT "Coppia di chiavi ECDSA generata. Handle Privato: "; pvtHandle; ", Pubblico: "; pubHandle
    
    ' Firma
    DIM messaggio$ = "Questo è un messaggio da firmare."
    DIM signature_bytes() = SIGN(messaggio$, pvtHandle)
    PRINT "Firma (HEX): "; TOHEX(signature_bytes())
    
    ' Verifica
    DIM esito = VERIFY(messaggio$, signature_bytes(), pubHandle)
    IF esito THEN PRINT "Verifica Firma: OK!" ELSE PRINT "Verifica Firma: ERRORE!"
    
    ' Generazione Chiavi per Cifratura (RSA 2048 bit)
    DIM rsaPvt, rsaPub
    KEYGEN "RSA", rsaPvt, rsaPub, 2048
    
    ' Cifratura Asimmetrica (con chiave Pubblica)
    DIM encrypted_rsa() = ENCRYPTASYM("Top Secret", rsaPub)
    
    ' Decifratura Asimmetrica (con chiave Privata)
    DIM decrypted_rsa_bytes() = DECRYPTASYM(encrypted_rsa(), rsaPvt)
    
    ' Trick per convertire array di byte UTF8 in stringa: Bytes -> Base64 -> String
    DIM decrypted_rsa$ = FROMBASE64(TOBASE64(decrypted_rsa_bytes()))
    IF decrypted_rsa$ = "Top Secret" THEN PRINT "Risultato RSA: OK!" ELSE PRINT "Risultato RSA: ERRORE!"

    ' Test Esportazione/Importazione Chiavi
    DIM pubKeyPem$ = KEYEXPORT(pubHandle)
    DIM newPubHandle
    KEYIMPORT(pubKeyPem$, newPubHandle)
    PRINT "Chiave pubblica esportata e re-importata. Nuovo Handle: "; newPubHandle
    
    ' Verifica firma con la chiave re-importata
    esito = VERIFY(messaggio$, signature_bytes(), newPubHandle)
    IF esito THEN PRINT "Verifica con Chiave Importata: OK!" ELSE PRINT "Verifica con Chiave Importata: ERRORE!"
    
    ' Pulizia risorse (handle non gestiti)
    KEYCLOSE pvtHandle
    KEYCLOSE pubHandle
    KEYCLOSE newPubHandle
    KEYCLOSE rsaPvt
    KEYCLOSE rsaPub
    PRINT

    ' --- 4. Test Utility e Funzioni Random ---
    PRINT "--- 4. Test Utility e Funzioni Random ---"
    PRINT "TOBASE64('abc'): "; TOBASE64("abc")
    PRINT "FROMBASE64('YWJj'): "; FROMBASE64("YWJj")
    PRINT "TOHEX('abc'):   "; TOHEX("abc")
    ' Decodifica manuale dei primi 3 byte da Hex
    PRINT "FROMHEX('616263'): "; CHR(FROMHEX(hex_string$)(0)); CHR(FROMHEX(hex_string$)(1)); CHR(FROMHEX(hex_string$)(2))
    
    ' Generazione numeri casuali crittograficamente sicuri
    PRINT "RANDOMBYTES(4):  "; TOHEX(RANDOMBYTES(4))
    PRINT "RANDOMINT(1, 10): "; RANDOMINT(1, 10)
    PRINT "RANDOMDOUBLE:    "; RANDOMDOUBLE(1.0)
    PRINT "RANDOMNORMAL:    "; RANDOMNORMAL()
    PRINT "RANDOMCRYPTOBYTES(4): "; TOHEX(RANDOMCRYPTOBYTES(4))
    PRINT
    
    ' --- NUOVA SEZIONE: 5. Test di Crittografia a Flusso (ChaCha20) ---
    PRINT "--- 5. Test di Crittografia a Flusso (ChaCha20) ---"
    DIM messaggio_chacha$ = "Questo è un messaggio segreto per la cifratura a flusso."
    ' ChaCha20 richiede una chiave a 32 byte e un nonce a 12 byte.
    DIM chacha_key() = RANDOMCRYPTOBYTES(32)
    DIM chacha_nonce() = RANDOMCRYPTOBYTES(12)
    ' Dati associati opzionali per il contesto di autenticazione (AEAD)
    DIM associated_data() = FROMHEX("010203") 
    PRINT "Messaggio originale: '"; messaggio_chacha$; "'"
    
    ' -- 5a. ChaCha20 semplice --
    PRINT "Cifratura/Decifratura con ChaCha20 semplice..."
    DIM cifrato_chacha() = CHACHA20ENCRYPT(messaggio_chacha$, chacha_key(), chacha_nonce())
    DIM decrittato_chacha_bytes() = CHACHA20DECRYPT(cifrato_chacha(), chacha_key(), chacha_nonce())
    IF FROMBASE64(TOBASE64(decrittato_chacha_bytes())) = messaggio_chacha$ THEN PRINT "Risultato ChaCha20 Semplice: OK!" ELSE PRINT "Risultato ChaCha20 Semplice: ERRORE!"

    ' -- 5b. ChaCha20-Poly1305 con autenticazione (AEAD) --
    PRINT "Cifratura/Decifratura con ChaCha20-Poly1305 (AEAD)..."
    DIM aead_result AS DICT OF STRING, ARRAY OF BYTE
    aead_result = CHACHA20POLY1305ENCRYPT(messaggio_chacha$, chacha_key(), chacha_nonce(), associated_data())
    PRINT "Testo cifrato AEAD (Base64): "; TOBASE64(aead_result("ciphertext"))
    PRINT "Tag di Autenticazione (HEX): "; TOHEX(aead_result("tag"))
    
    DIM decrittato_aead_bytes() = CHACHA20POLY1305DECRYPT(aead_result("ciphertext"), aead_result("tag"), chacha_key(), chacha_nonce(), associated_data())
    IF FROMBASE64(TOBASE64(decrittato_aead_bytes())) = messaggio_chacha$ THEN PRINT "Risultato ChaCha20-Poly1305: OK!" ELSE PRINT "Risultato ChaCha20-Poly1305: ERRORE!"
        
    PRINT "--- TEST COMPLETATO ---"
END SUB