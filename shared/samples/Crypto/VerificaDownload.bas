' FILE: Crypto/VerificaDownload.bas
' SCENARIO DIDATTICO — verificare un pacchetto scaricato prima di installarlo.
'
' Sequenza reale di un installer:
'   1) il pacchetto arriva su disco e si calcola il suo digest SHA-256;
'   2) il digest viene confrontato con quello pubblicato nel manifest;
'   3) il manifest è autenticato con un HMAC (segreto condiviso) per escludere
'      manifest falsificati;
'   4) se il file viene alterato, i controlli devono FALLIRE (test negativo);
'   5) il token di download è protetto con un hash di password verificabile.
'
' Domini: CRYPTOGRAPHY + IO + STRINGS. Tutto deterministico (segreti e testi fissi).
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT IO

SUB Main()
    IF NOT IO.DIREXISTS("work") THEN MKDIR "work"

    ' ── 1. Il pacchetto scaricato ────────────────────────────────────────────
    DIM contenuto AS STRING
    contenuto = "pacchetto gmb-demo 1.0" + CHR(10) + "payload" + CHR(10)
    DIM h AS LONG
    h = IO.FCREATE("work/pacchetto.zip.txt")
    IO.FWRITE(h, contenuto)
    IO.FCLOSE(h)
    PRINT "== 1. Pacchetto su disco =="
    PRINT "byte scritti: "; LEN(contenuto)

    ' ── 2. Digest del file, in esadecimale ───────────────────────────────────
    ' STRHASHFILE legge il FILE; HASHFILE restituisce i byte grezzi del digest.
    DIM digestFile AS STRING
    digestFile = STRHASHFILE("work/pacchetto.zip.txt", "SHA256")
    PRINT "== 2. Digest SHA-256 del file =="
    PRINT "sha256: "; digestFile
    ' HASHFILE ritorna il digest come STRINGA ESADECIMALE: 2 caratteri per byte.
    DIM byteDigest AS STRING
    byteDigest = HASHFILE("work/pacchetto.zip.txt", "SHA256")
    PRINT "lunghezza digest: "; LEN(byteDigest); " caratteri hex (attesi 64 = 32 byte)"

    ' ── 3. Manifest + autenticazione con HMAC ────────────────────────────────
    ' Il manifest dichiara il digest; l'HMAC lo lega a un segreto condiviso, così
    ' un attaccante non può sostituire il file E il digest senza il segreto.
    DIM manifest AS STRING
    manifest = "pacchetto.zip.txt " + digestFile
    DIM segreto AS STRING
    segreto = "chiave-del-server-di-rilascio"
    DIM firmaManifest AS STRING
    firmaManifest = HMACSHA256(manifest, segreto)   ' 64 caratteri esadecimali
    PRINT "== 3. Manifest autenticato =="
    PRINT "hmac: "; LEFT(firmaManifest, 32); "..."

    ' Verifica: ricalcolando l'HMAC sul manifest si deve ottenere lo stesso valore.
    DIM ricalcolato AS STRING
    ricalcolato = HMACSHA256(manifest, segreto)
    PRINT "manifest integro: "; (ricalcolato = firmaManifest)
    ' Con un segreto sbagliato l'HMAC cambia: il manifest non è più autentico.
    DIM firmaFalsa AS STRING
    firmaFalsa = HMACSHA256(manifest, "segreto-sbagliato")
    PRINT "manifest falsificato respinto: "; (firmaFalsa <> firmaManifest)

    ' ── 4. Test negativo: file alterato ──────────────────────────────────────
    ' Un byte in più cambia completamente il digest: il confronto col manifest
    ' deve fallire (è il controllo che blocca l'installazione).
    h = IO.FCREATE("work/pacchetto.zip.txt")
    IO.FWRITE(h, contenuto + "!")
    IO.FCLOSE(h)
    DIM digestAlterato AS STRING
    digestAlterato = STRHASHFILE("work/pacchetto.zip.txt", "SHA256")
    PRINT "== 4. File alterato =="
    PRINT "digest cambiato: "; (digestAlterato <> digestFile)
    PRINT "installazione consentita: "; (digestAlterato = digestFile)

    ' ── 5. Token di download ─────────────────────────────────────────────────
    ' Il token non si conserva in chiaro: hash di password, verificabile.
    DIM token AS STRING
    token = PASSWORDHASH("token-utente-42")
    PRINT "== 5. Token =="
    PRINT "chiave corretta: "; PASSWORDVERIFY("token-utente-42", token)
    PRINT "chiave errata:  "; PASSWORDVERIFY("token-sbagliato", token)

    KILL "work/pacchetto.zip.txt"
END SUB
