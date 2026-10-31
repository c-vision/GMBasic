' FILE: Network/NetSslConnectFailures.bas
' ======================================================================================================================
' GUIDA TEORICA: SICUREZZA E VALIDAZIONE DEI CERTIFICATI SSL/TLS
' ======================================================================================================================
' Questo programma testa una delle funzionalità di sicurezza più importanti del comando `SSL CONNECT`: la validazione
' automatica dei certificati digitali dei server.
'
' 1. IL RUOLO DEL CERTIFICATO SSL/TLS
'    - Quando ci si connette a un server sicuro (HTTPS), il server presenta un certificato digitale come "carta
'      d'identità". Questo certificato è firmato da un'autorità di certificazione (CA) attendibile (es. Let's
'      Encrypt, DigiCert).
'
' 2. IL PROCESSO DI VALIDAZIONE
'    - Il comando `SSL CONNECT` esegue una serie di controlli rigorosi sul certificato ricevuto dal server,
'      simili a quelli eseguiti da un browser web moderno. Se anche uno solo di questi controlli fallisce, la
'      connessione viene rifiutata per proteggere l'utente.
'    - Controlli Fondamentali:
'      a) Catena di Fiducia (Trust Chain): Verifica che il certificato sia stato emesso da una CA presente
'         nell'elenco di quelle attendibili del sistema operativo. Un certificato "auto-firmato" (self-signed),
'         in cui il server si firma da solo, fallirà questo controllo.
'      b) Scadenza (Expiration): Controlla che la data e l'ora correnti rientrino nel periodo di validità
'         del certificato. Un certificato scaduto viene rifiutato.
'      c) Corrispondenza del Nome Host (Hostname Mismatch): Verifica che il nome host a cui ci si sta
'         connettendo (es. "www.google.com") sia elencato tra i nomi validi nel certificato. Questo
'         previene attacchi "man-in-the-middle" in cui un malintenzionato presenta un certificato valido
'         ma emesso per un altro sito.
'
' 3. SCOPO DEL TEST
'    - Questo programma si connette deliberatamente a una serie di server (forniti da badssl.com) che presentano
'      certificati con problemi specifici e noti.
'    - L'obiettivo è verificare che `SSL CONNECT` fallisca correttamente in ognuno di questi scenari, dimostrando
'      che il meccanismo di validazione è attivo e robusto. Un "SUCCESSO" in questo test significa che la
'      connessione è fallita come previsto.
' ======================================================================================================================

' DESCRIZIONE:
'   Verifica che SSL CONNECT fallisca correttamente quando tenta di
'   connettersi a server con certificati SSL invalidi.
IMPORT NET

' Subroutine helper per eseguire un singolo test di connessione
SUB TestConnection(host$, description$)
    PRINT "--------------------------------------------------"
    PRINT "Test: "; description$
    PRINT "Host: "; host$
    
    ' Dichiara l'handle per la connessione
    DIM handle AS LONG
    ' Tenta la connessione con un timeout di 5 secondi
    SSL CONNECT host$, handle, 443, 5000
    
    ' Il test ha successo se l'handle restituito è 0 (connessione fallita)
    IF handle = 0 THEN
        PRINT "RISULTATO: SUCCESSO! Connessione fallita come previsto."
    ELSE
        ' Se l'handle è > 0, la connessione è riuscita, il che è un errore di sicurezza
        PRINT "RISULTATO: FALLITO! La connessione è riuscita ma non doveva."
        ' Chiude la connessione aperta erroneamente
        TCP CLOSE handle
    END IF
    PRINT
END SUB

' Programma principale
SUB Main
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ NetSslConnectFailures - Test di gestione errori SSL/TLS (Client)             ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Esegue i test su vari tipi di certificati invalidi
    TestConnection("self-signed.badssl.com", "Certificato Auto-Firmato")
    TestConnection("expired.badssl.com", "Certificato Scaduto")
    TestConnection("wrong.host.badssl.com", "Hostname non corrispondente")
    
    PRINT "--- TEST DI FALLIMENTO COMPLETATI ---"
END SUB
