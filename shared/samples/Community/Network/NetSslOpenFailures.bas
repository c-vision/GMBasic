' FILE: Network/NetSslOpenFailures.bas
' ======================================================================================================================
' GUIDA TEORICA: AVVIO DI SERVER SICURI (SSL/TLS) E GESTIONE ERRORI
' ======================================================================================================================
' Questo programma testa la robustezza del comando `SSL OPEN` nella gestione di errori di configurazione comuni,
' in particolare quelli legati al certificato digitale.
'
' 1. IL COMANDO `SSL OPEN`
'    - Sintassi: SSL OPEN <port>, <handleVar>, <certPath$>, [<certPass$>]
'    - Funzionamento: Avvia un server TCP in ascolto su una porta specifica, ma lo avvolge in un layer di
'      sicurezza SSL/TLS. Per farlo, richiede un certificato digitale (tipicamente in formato .pfx o .p12)
'      che contiene sia la chiave pubblica che la chiave privata del server.
'
' 2. IMPORTANZA DEL CERTIFICATO
'    - A differenza di `SSL CONNECT`, che *valida* il certificato di un server remoto, `SSL OPEN` *utilizza*
'      un certificato locale per autenticarsi presso i client che si connettono.
'    - La validità di questo certificato è fondamentale per la sicurezza della comunicazione.
'
' 3. SCENARI DI ERRORE COMUNI
'    - File Non Trovato: Se il percorso del file del certificato specificato in `certPath$` non esiste,
'      l'avvio del server deve fallire in modo controllato.
'    - Password Errata: I file di certificato contenenti una chiave privata sono quasi sempre protetti da
'      una password. Se la `certPass$` fornita è errata, il sistema non può caricare la chiave privata e
'      l'avvio del server deve fallire.
'    - File Corrotto o Formato Non Valido: Se il file non è un certificato valido, il caricamento fallirà.
'
' 4. SCOPO DEL TEST
'    - Questo programma tenta di avviare un server SSL con parametri deliberatamente errati: un file
'      inesistente e una password sbagliata.
'    - L'obiettivo è verificare che il comando `SSL OPEN` non causi un crash del programma, ma gestisca
'      l'errore in modo pulito, restituendo `0` nell'handle, permettendo al programma di continuare o
'      terminare in modo controllato.
' ======================================================================================================================

' DESCRIZIONE:
'   Verifica che SSL OPEN fallisca in modo controllato quando
'   vengono forniti parametri del certificato non validi.
IMPORT NET

' Subroutine helper per eseguire un singolo test di avvio del server
SUB TestOpen(port, certPath$, certPass$, description$)
    PRINT "--------------------------------------------------"
    PRINT "Test: "; description$
    DIM handle AS LONG
    
    ' Tenta di avviare il server SSL con i parametri forniti
    SSL OPEN port, handle, certPath$, certPass$
    
    ' Se l'handle è 0, il server non si è avviato, il che è il comportamento atteso per questo test
    IF handle = 0 THEN
        PRINT "RISULTATO: SUCCESSO! Avvio del server fallito come previsto."
    ELSE
        ' Se l'handle > 0, il server si è avviato, indicando un potenziale problema di sicurezza
        PRINT "RISULTATO: FALLITO! Il server si è avviato ma non doveva."
        ' Chiude il server avviato erroneamente
        TCP CLOSE handle
    END IF
    PRINT
END SUB

' Programma principale
SUB Main
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ NetSslOpenFailures - Test di gestione errori SSL/TLS (Server)                ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Esegue i test su scenari di fallimento comuni
    TestOpen(8443, "certificato_inesistente.pfx", "test", "Certificato non trovato")
    TestOpen(8444, "server.pfx", "password_sbagliata", "Password del certificato errata")
    
    PRINT "--- TEST DI FALLIMENTO COMPLETATI ---"
    ' Nota per l'utente che esegue il test
    PRINT "(Assicurarsi che esista un file 'server.pfx' con password 'test' per il secondo test)"
END SUB
