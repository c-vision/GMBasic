' FILE: Types/TypesEnums.bas
' ======================================================================================================================
' GUIDA TEORICA: ENUMERAZIONI (ENUM)
' ======================================================================================================================
' Le enumerazioni (`ENUM`) forniscono un modo per creare un nuovo tipo di dato i cui valori possibili sono un set
' limitato di costanti nominate. Questo migliora la leggibilità e la robustezza del codice rispetto all'uso di
' semplici numeri ("magic numbers").
'
' 1. SINTASSI E DEFINIZIONE
'    - `[PUBLIC|PRIVATE] ENUM NomeTipo [AS TipoBase] ... END ENUM`
'    - `NomeTipo`: Il nome del nuovo tipo di dato creato.
'    - `TipoBase` (Opzionale): Il tipo numerico sottostante (es. `INTEGER`, `BYTE`, `LONG`). Se omesso, il default è `INTEGER`.
'
' 2. VALORI DEI MEMBRI
'    - Valore Esplicito: `Membro = valore`.
'    - Valore Implicito: Se un membro non ha un valore esplicito, il compilatore gli assegna automaticamente il
'      valore del membro precedente + 1. Il primo membro, se implicito, ha valore 0.
'
' 3. UTILIZZO
'    - Dichiarazione: `DIM miaVar AS NomeTipo`.
'    - Accesso ai Membri: `NomeTipo.Membro` (es. `StatoOrdine.Spedito`).
'    - Assegnazione e Confronto: Le variabili di tipo ENUM possono essere assegnate e confrontate con i membri
'      dell'enum o con valori numerici del tipo base compatibile.
'
' 4. VISIBILITÀ
'    - `PUBLIC`: L'enum è visibile e utilizzabile in altri moduli (se importato).
'    - `PRIVATE` (o default): L'enum è visibile solo all'interno del file in cui è definito.
' ======================================================================================================================

' Test 1: Enum Pubblico con tipo base INTEGER (default) e valori misti (espliciti/impliciti).
PUBLIC ENUM StatoOrdine AS INTEGER
    ' Valore esplicito
    Nuovo = 1
    ' Valore implicito: sarà Nuovo + 1 = 2
    InLavorazione
    ' Valore esplicito
    Spedito = 10
    ' Valore implicito: sarà Spedito + 1 = 11
    Consegnato
    ' Valore esplicito
    Chiuso = 99
END ENUM

' Test 2: Enum Privato con tipo base BYTE.
PRIVATE ENUM Priorita AS BYTE
    Bassa = 1
    Media = 5
    Alta = 10
    Critica = 255 ' Valore massimo per un BYTE
END ENUM

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TypesEnums - Test delle Enumerazioni                                    ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Test 3: Utilizzo dei valori di un enum
    PRINT "--- Test 1: Utilizzo dei valori ENUM ---"
    ' Dichiara una variabile del tipo enum StatoOrdine e la inizializza.
    DIM ordine AS StatoOrdine = StatoOrdine.Nuovo
    PRINT "Stato iniziale:", ordine; "(Atteso: 1)"
    
    ' Assegna un valore implicito
    ordine = StatoOrdine.InLavorazione
    ' Verifica che il compilatore abbia assegnato il valore corretto (2)
    IF ordine = 2 THEN
        PRINT "OK: StatoOrdine.InLavorazione ha valore 2 (auto-incremento)"
    ELSE
        PRINT "FAIL: StatoOrdine.InLavorazione ha valore ", ordine
    END IF
    PRINT
    
    ' Test 4: Utilizzo di un enum privato con tipo base diverso
    PRINT "--- Test 2: Enum Privato con Tipo Base BYTE ---"
    DIM p AS Priorita = Priorita.Critica
    PRINT "Priorità Critica:", p; "(Atteso: 255)"
    PRINT

    ' Test 5: Casting implicito da enum a tipo numerico
    PRINT "--- Test 3: Casting Implicito a INTEGER ---"
    ' Una variabile di tipo numerico può essere assegnata direttamente da un membro dell'enum.
    DIM valoreIntero AS INTEGER = StatoOrdine.Spedito
    PRINT "Valore di StatoOrdine.Spedito assegnato a INTEGER:", valoreIntero; "(Atteso: 10)"
    PRINT
    
    ' VERIFICA DI COMPLETEZZA
    ' Aggiunto test per verificare l'assegnazione da un valore numerico a una variabile enum.
    PRINT "--- Test 4: Assegnazione da Valore Numerico a ENUM ---"
    ordine = 11 ' Assegna un valore numerico valido
    IF ordine = StatoOrdine.Consegnato THEN
        PRINT "OK: Assegnazione da valore numerico 11 a StatoOrdine riuscita."
    ELSE
        PRINT "FAIL: Assegnazione da valore numerico fallita."
    END IF
    PRINT
    
    PRINT "--- Test completato ---"
END SUB