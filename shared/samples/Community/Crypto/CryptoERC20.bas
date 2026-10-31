' FILE: Crypto/CryptoERC20.bas
' =================================================================================================
'       GUIDA TEORICA: INTERAZIONE CON SMART CONTRACT (ERC-20)
' =================================================================================================
'
' Questo esempio dimostra l'uso avanzato del namespace `BLOCKCHAIN` per interagire con Smart Contract
' standard su una rete Ethereum-compatibile, sfruttando il supporto per interfacce tipizzate.
'
' 1. IL PATTERN 'CONTRACT' (PROXY GENERATOR)
'    La funzione `CONTRACT(address, interfaceName)` è il cuore dell'interoperabilità.
'    - Non richiede la definizione manuale dell'ABI JSON.
'    - Utilizza le definizioni di `INTERFACE` presenti nel linguaggio (es. `IERC20`).
'    - Restituisce un oggetto "proxy" dinamico. Quando si chiama un metodo su questo oggetto,
'      il runtime intercetta la chiamata, codifica gli argomenti e invia la richiesta RPC.
'
' 2. L'INTERFACCIA STANDARD IERC20
'    GM-BASIC include nativamente la definizione dello standard ERC-20:
'    (definita come built-in nel namespace BLOCKCHAIN)
'
' 3. CODIFICA E DECODIFICA ABI
'    Oltre all'astrazione di alto livello, vengono mostrate le funzioni `ABIENCODE` e `ABIDECODE`
'    per operazioni di basso livello o debug.
'
' =================================================================================================

IMPORT BLOCKCHAIN
IMPORT CONSOLE

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ CryptoERC20 - Interazione con Smart Contract                   ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    ' 1. CONFIGURAZIONE
    ' Usiamo l'indirizzo del contratto USDT su Ethereum Mainnet (esempio)
    DIM usdtAddress$ = "0xdAC17F958D2ee523a2206206994597C13D831ec7"
    ' Indirizzo di un holder fittizio (Binance Hot Wallet)
    DIM holderAddress$ = "0xF977814e90dA44bFA03b6295A0616a897441aceC"
    ' URL RPC (In un caso reale, useresti Infura o Alchemy)
    DIM rpcUrl$ = "https://mainnet.infura.io/v3/YOUR-PROJECT-ID"

    PRINT "Configurazione:"
    PRINT "Contratto (USDT): "; usdtAddress$
    PRINT "Holder:           "; holderAddress$
    PRINT

    ' 2. CREAZIONE DEL PROXY DEL CONTRATTO
    ' Qui avviene la magia: creiamo un oggetto che rispetta l'interfaccia IERC20
    ' collegato all'indirizzo fisico sulla blockchain.
    PRINT "Creazione proxy per interfaccia 'IERC20'..."
    DIM token AS IERC20
    
    ' NOTA: In un ambiente reale, questa chiamata inizializzerebbe la connessione RPC.
    ' Nel simulatore attuale, preparerà l'oggetto per generare i payload corretti.
    token = BLOCKCHAIN.CONTRACT(usdtAddress$, "IERC20", rpcUrl$)
    PRINT "Proxy creato con successo."
    PRINT

    ' 3. INTERAZIONE (LETTURA STATO - CALL)
    PRINT "--- Lettura Dati Contratto (Simulazione) ---"
    
    ' Chiamate ai metodi dell'interfaccia come se fossero metodi locali
    PRINT "Chiamata a Name()..."
    ' DIM name$ = token.Name() ' Questo invierebbe eth_call con data 0x06fdde03 (keccak di name())
    PRINT "  -> Payload inviato: 0x06fdde03" 
    PRINT "  -> Risultato decodificato (ipotetico): Tether USD"
    
    PRINT "Chiamata a Symbol()..."
    ' DIM symbol$ = token.Symbol()
    PRINT "  -> Payload inviato: 0x95d89b41"
    PRINT "  -> Risultato decodificato (ipotetico): USDT"
    
    PRINT "Chiamata a Decimals()..."
    ' DIM decimals% = token.Decimals()
    PRINT "  -> Payload inviato: 0x313ce567"
    PRINT "  -> Risultato decodificato (ipotetico): 6"
    PRINT

    ' 4. INTERAZIONE CON ARGOMENTI
    PRINT "--- Chiamata con Argomenti ---"
    PRINT "Chiamata a BalanceOf("; holderAddress$; ")..."
    
    ' Internamente, il runtime calcola l'hash di "balanceOf(address)" -> 0x70a08231
    ' e codifica l'indirizzo con padding a 32 byte.
    ' DIM balance$ = token.BalanceOf(holderAddress$)
    
    PRINT "  -> Firma Metodo (Keccak): 0x70a08231"
    PRINT "  -> Argomento Codificato:  000000000000000000000000f977814e90da44bfa03b6295a0616a897441acec"
    PRINT "  -> Payload Completo:      0x70a08231000000000000000000000000f977814e90da44bfa03b6295a0616a897441acec"
    PRINT

    ' 5. UTILITÀ DI BASSO LIVELLO (ABI ENCODING)
    PRINT "--- Test Low-Level ABI Encoding ---"
    ' Dimostriamo come fare manualmente ciò che il proxy fa in automatico
    
    DIM signature$ = "transfer(address,uint256)"
    DIM to$ = "0x1234567890123456789012345678901234567890"
    DIM amount$ = "1000000000" ' 1000 Token (con 6 decimali)
    
    PRINT "Codifica manuale di 'transfer'..."
    DIM encodedData$ = ABIENCODE(signature$, { to$, amount$ })
    
    PRINT "Firma attesa (a9059cbb): "; LEFT(encodedData$, 10) ' 0x + 4 bytes
    PRINT "Dati completi:"
    PRINT encodedData$
    PRINT

    PRINT "--- TEST COMPLETATO ---"
END SUB
