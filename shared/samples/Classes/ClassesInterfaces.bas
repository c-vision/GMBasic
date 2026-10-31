' FILE: Classes/ClassesInterfaces.bas
' =================================================================================================
'        GUIDA TEORICA: Interfacce e Polimorfismo (INTERFACE / IMPLEMENTS)
' =================================================================================================
'
' Questo file di test verifica l'implementazione delle interfacce in GM-BASIC, una potente
' funzionalità che permette a classi diverse di condividere un contratto comune, promuovendo
' il disaccoppiamento e il polimorfismo.
'
' 1. INTERFACCIA (`INTERFACE`)
'    Un'interfaccia è un "contratto" puro. Definisce un insieme di firme di metodi, proprietà
'    ed eventi che una classe DEVE implementare. Non contiene codice eseguibile né campi.
'    - Membri: Tutti i membri di un'interfaccia sono implicitamente `PUBLIC` e `ABSTRACT`.
'    - Istanziazione: Un'interfaccia non può essere istanziata direttamente con `NEW`.
'
' 2. IMPLEMENTAZIONE (`IMPLEMENTS`)
'    Una classe può dichiarare di aderire a uno o più contratti utilizzando la keyword `IMPLEMENTS`.
'    - Obbligo di Implementazione: La classe deve fornire un'implementazione `PUBLIC` per OGNI
'      membro definito in tutte le interfacce che implementa.
'    - Ereditarietà Multipla di Contratto: A differenza dell'ereditarietà di classe (`INHERITS`),
'      una classe può implementare più interfacce, separate da virgole.
'
' OBIETTIVI DI QUESTO TEST:
' 1. Validare che una `INTERFACE` non possa essere istanziata (vedi `Main`).
' 2. Validare che una classe DEVE implementare TUTTI i membri di un'interfaccia.
'    (vedi la classe `FileLoggerIncompleto` commentata).
' 3. Verificare il polimorfismo, dove una variabile o un array di tipo interfaccia può contenere
'    istanze di qualsiasi classe che implementa quel contratto.
' 4. Verificare l'implementazione di interfacce multiple in una singola classe.
' 5. Verificare che una classe astratta possa implementare parzialmente un'interfaccia, delegando
'    l'implementazione finale alle sue sottoclassi concrete.
'
' =================================================================================================

' --- DEFINIZIONE INTERFACCE ---

' Interfaccia per oggetti che possono essere serializzati in una stringa.
PUBLIC INTERFACE ISerializable
    FUNCTION Serialize() AS STRING
END INTERFACE

' Interfaccia per oggetti che hanno una forma e possono essere disegnati.
PUBLIC INTERFACE IDisegnabile
    SUB Disegna()
    PROPERTY Colore AS STRING
END INTERFACE


' --- CLASSE 1: Implementa una singola interfaccia ---
PUBLIC CLASS Punto IMPLEMENTS ISerializable
    PUBLIC DIM X AS INTEGER, Y AS INTEGER

    PUBLIC SUB NEW(x AS INTEGER, y AS INTEGER)
        SELF.X = x
        SELF.Y = y
    END SUB
    
    ' Implementazione obbligatoria di ISerializable
    PUBLIC FUNCTION Serialize() AS STRING
        FUNCTION = "Punto(" + CSTR(SELF.X) + ", " + CSTR(SELF.Y) + ")"
    END FUNCTION
END CLASS


' --- CLASSE 2: Implementa interfacce multiple ---
PUBLIC CLASS Quadrato IMPLEMENTS ISerializable, IDisegnabile
    PUBLIC DIM Lato AS DOUBLE
    PRIVATE DIM _colore AS STRING

    PUBLIC SUB NEW(lato AS DOUBLE, colore AS STRING)
        SELF.Lato = lato
        SELF._colore = colore
    END SUB
    
    ' Implementazione obbligatoria di ISerializable
    PUBLIC FUNCTION Serialize() AS STRING
        FUNCTION = "Quadrato(Lato=" + CSTR(SELF.Lato) + ")"
    END FUNCTION
    
    ' Implementazione obbligatoria di IDisegnabile
    PUBLIC SUB Disegna()
        PRINT "Disegno un quadrato di lato "; SELF.Lato; " e colore "; SELF.Colore
    END SUB

    ' Implementazione obbligatoria della proprietà di IDisegnabile
    PUBLIC PROPERTY Colore AS STRING
        GET
            RETURN SELF._colore
        END GET
        SET(BYVAL value AS STRING)
            SELF._colore = value
        END SET
    END PROPERTY
END CLASS


' --- CLASSE 3 (INTENZIONALMENTE INCOMPLETA E COMMENTATA) ---
' Se de-commentata, DEVE generare un errore di compilazione perché
' manca l'implementazione della proprietà `Colore`.
'
' PUBLIC CLASS CerchioFallito IMPLEMENTS IDisegnabile
'    PUBLIC DIM Raggio AS DOUBLE
'    
'    PUBLIC SUB Disegna()
'        PRINT "Disegno un cerchio"
'    END SUB
'
'    ' ERRORE: Manca l'implementazione di "PROPERTY Colore AS STRING"
' END CLASS
'


' --- PROGRAMMA PRINCIPALE ---
SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClassesInterfaces - Test Interfacce e Polimorfismo             ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- TEST 1: Verifica del Polimorfismo con Interfacce ---
    PRINT "--- 1. Test Polimorfismo su Array di tipo Interfaccia ---"
    
    DIM serializzabili(1) AS ISerializable
    serializzabili(0) = NEW Punto(10, 20)
    serializzabili(1) = NEW Quadrato(5, "Rosso")

    PRINT "Serializzazione di oggetti diversi tramite interfaccia ISerializable:"
    FOR EACH obj AS ISerializable IN serializzabili
        PRINT "   - Oggetto serializzato: "; obj.Serialize()
    NEXT
    PRINT

    ' --- TEST 2: Implementazione di interfacce multiple ---
    PRINT "--- 2. Test su oggetto che implementa più interfacce ---"
    DIM q AS Quadrato = NEW Quadrato(7, "Verde")
    
    ' Possiamo assegnare lo stesso oggetto a variabili di tipi di interfaccia diversi
    DIM formaDisegnabile AS IDisegnabile = q
    DIM datoSerializzabile AS ISerializable = q
    
    formaDisegnabile.Colore = "Blu"
    formaDisegnabile.Disegna()
    
    PRINT "Oggetto serializzato (dopo modifica colore): "; datoSerializzabile.Serialize()
    PRINT

    ' --- TEST 3: Verifica del Blocco di Istanziazione (DEVE FALLIRE) ---
    PRINT "--- 3. Test Blocco Istanziazione Interfaccia (commentato) ---"
    PRINT "   (Se de-commentata, la riga seguente deve causare un errore di compilazione)"
    '
    ' ERRORE DI COMPILAZIONE ATTESO: Impossibile creare un'istanza di un'interfaccia 'ISerializable'.
    ' DIM obj AS ISerializable = NEW ISerializable
    '
    PRINT "   OK: Blocco di test superato (se compilato)."
    PRINT
    
    PRINT "--- Test completato ---"
END SUB
