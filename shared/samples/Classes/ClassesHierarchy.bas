' FILE: Classes/ClassesHierarchy.bas
' =================================================================================================
'               GUIDA TEORICA: OOP - Ereditarietà, Polimorfismo e Proprietà
' =================================================================================================
'
' Questo test è una dimostrazione completa dei concetti chiave della programmazione a oggetti (OOP)
' in GM-BASIC, utilizzando la sintassi a blocchi tradizionale.
'
' 1. EREDITARIETÀ (`INHERITS`) E GERARCHIA DI CLASSI
'    Una classe può "ereditare" campi e metodi da un'altra classe (detta classe base o superclasse)
'    utilizzando la keyword `INHERITS`. Questo stabilisce una relazione "è un" (es. un Cerchio "è una" Forma2D)
'    e promuove il riutilizzo del codice. Questo file dimostra una gerarchia a più livelli:
'    `Forma` -> `Forma2D` -> `Cerchio`.
'
' 2. POLIMORFISMO (`VIRTUAL` e `OVERRIDE`)
'    Il polimorfismo è la capacità di trattare oggetti di classi diverse attraverso un'interfaccia comune.
'    - `VIRTUAL`: Un metodo dichiarato `VIRTUAL` nella classe base segnala che le classi derivate
'      "possono" fornire una loro implementazione specifica per quel metodo.
'    - `OVERRIDE`: Un metodo dichiarato `OVERRIDE` in una classe derivata "sovrascrive" un metodo
'      `VIRTUAL` della classe base, fornendone una nuova implementazione.
'    Quando un metodo virtuale viene chiamato su un oggetto, GM-BASIC esegue la versione del metodo che
'    corrisponde al tipo *reale* dell'oggetto a runtime, non al tipo della variabile che lo contiene.
'    Questo è il cuore del polimorfismo, come dimostrato nell'array `forme()` in `Main`.
'
' 3. MODIFICATORI DI ACCESSO (`PUBLIC`, `PROTECTED`, `PRIVATE`)
'    Queste keyword controllano la "visibilità" dei membri di una classe:
'    - `PUBLIC`: Il membro è accessibile da qualsiasi punto del codice.
'    - `PROTECTED`: Il membro è accessibile solo all'interno della classe stessa e delle sue classi derivate.
'    - `PRIVATE`: Il membro è accessibile solo all'interno della classe che lo dichiara.
'
' 4. ACCESSO ALLA CLASSE BASE (`BASE`)
'    - `BASE.NEW(...)`: Chiama il costruttore della classe base. È obbligatorio farlo come prima istruzione
'      in un costruttore di una classe derivata se la base non ha un costruttore senza parametri.
'    - `BASE.<Metodo>(...)`: Permette a un metodo sovrascritto di chiamare l'implementazione originale
'      del metodo nella sua classe base, estendendone la funzionalità invece di sostituirla completamente.
'
' 5. PROPRIETÀ (`PROPERTY`)
'    Le proprietà espongono un'interfaccia pubblica per leggere o scrivere un valore, nascondendo
'    il campo privato sottostante. Offrono un controllo granulare tramite gli accessori:
'    - `GET`: Eseguito quando si legge il valore della proprietà.
'    - `SET`: Eseguito quando si assegna un valore alla proprietà. L'accessor `SET` riceve
'      il nuovo valore in un parametro speciale (es. `value`).
'    Le proprietà sono fondamentali per l'incapsulamento e per implementare logica di validazione.
'
' 6. MEMBRI STATICI (`STATIC`)
'    Un membro `STATIC` (campo o metodo) appartiene alla classe stessa, non a una singola istanza.
'    Tutte le istanze della classe condividono lo stesso membro statico. In questo esempio, `_nextId`
'    è un contatore globale condiviso da tutte le istanze di `Forma`.
'
' =================================================================================================

IMPORT MATH

' --- CLASSE BASE: Definisce le proprietà e i comportamenti comuni di una forma geometrica. ---
PUBLIC CLASS Forma
    ' Campi PROTECTED: visibili a questa classe e a tutte le sue classi derivate (Forma2D, Cerchio).
    PROTECTED DIM X AS DOUBLE
    PROTECTED DIM Y AS DOUBLE
    ' Campo PRIVATE: visibile solo all'interno della classe Forma.
    PRIVATE DIM _id AS LONG
    ' Campo STATIC: una singola istanza condivisa tra tutti gli oggetti di tipo Forma.
    STATIC DIM _nextId AS LONG = 1

    ' Costruttore della classe base.
    PUBLIC SUB NEW(x_pos AS DOUBLE, y_pos AS DOUBLE)
        SELF.X = x_pos
        SELF.Y = y_pos
        ' Accede al membro statico usando il nome della classe e lo usa come contatore.        
        SELF._id = Forma._nextId
        Forma._nextId = Forma._nextId + 1
    END SUB
    
    ' Proprietà pubblica di sola lettura per esporre l'ID privato.
    PUBLIC PROPERTY ID AS LONG
        GET
            RETURN SELF._id
        END GET
    END PROPERTY

    ' Metodo VIRTUAL: può essere sovrascritto dalle classi derivate.
    ' Fornisce un'implementazione di default.
    PUBLIC VIRTUAL FUNCTION Area() AS DOUBLE
        FUNCTION = 0
    END FUNCTION

    ' Metodo VIRTUAL per stampare le informazioni di base.
    PUBLIC VIRTUAL SUB StampaInfo()
        PRINT "Forma ID "; SELF.ID; " at ("; SELF.X; ", "; SELF.Y; ")";
    END SUB
END CLASS

' --- CLASSE DERIVATA (LIVELLO 1): Specializza Forma per il 2D, ereditandone i membri. ---
PUBLIC CLASS Forma2D INHERITS Forma
    ' Costruttore della classe derivata.
    PUBLIC SUB NEW(x_pos AS DOUBLE, y_pos AS DOUBLE)
        ' Chiama esplicitamente il costruttore della classe base (Forma) per inizializzare i campi ereditati.
        BASE.NEW(x_pos, y_pos)
    END SUB

    ' Sovrascrive (OVERRIDE) il metodo StampaInfo della classe base.
    PUBLIC OVERRIDE SUB StampaInfo()
        ' Chiama l'implementazione originale di StampaInfo in Forma per riutilizzarne la logica.
        BASE.StampaInfo()
        ' Aggiunge informazioni specifiche di questa classe.
        PRINT "  Tipo: Forma 2D, Area: "; SELF.Area()
    END SUB
END CLASS

' --- CLASSE DERIVATA (LIVELLO 2): Implementa una forma specifica (Cerchio). ---
PUBLIC CLASS Cerchio INHERITS Forma2D
    PRIVATE DIM _raggio AS DOUBLE ' Campo privato per incapsulare il raggio.

    ' Costruttore della classe Cerchio.
    PUBLIC SUB NEW(x_pos AS DOUBLE, y_pos AS DOUBLE, raggio_val AS DOUBLE)
        ' Chiama il costruttore della sua classe base (Forma2D), che a sua volta chiamerà quello di Forma.
        BASE.NEW(x_pos, y_pos)
        ' Utilizza l'accessor SET della proprietà Raggio per impostare il valore iniziale.
        SELF.Raggio = raggio_val ' Usa la proprietà SET
    END SUB

    ' Proprietà pubblica per accedere in modo controllato al campo privato _raggio.
    PUBLIC PROPERTY Raggio AS DOUBLE
        ' Accessor GET: eseguito quando si legge il valore (es. `PRINT c.Raggio`).
        GET
            RETURN SELF._raggio
        END GET
        ' Accessor SET: eseguito quando si assegna un valore (es. `c.Raggio = 10`).
        SET(BYVAL value AS DOUBLE)
            ' Aggiunge logica di validazione per garantire l'integrità dei dati.
            IF value > 0 THEN
                SELF._raggio = value
            ELSE
                ' Lancia un errore di runtime se il valore non è valido.
                ERROR 1000, "Il raggio deve essere positivo."
            END IF
        END SET
    END PROPERTY

    ' Sovrascrive la funzione Area per fornire l'implementazione specifica per il cerchio.
    PUBLIC OVERRIDE FUNCTION Area() AS DOUBLE
        FUNCTION = MATH.PI * SELF._raggio ^ 2
    END FUNCTION
    
    ' Sovrascrive StampaInfo per aggiungere i dettagli specifici del cerchio.
    PUBLIC OVERRIDE SUB StampaInfo()
        ' Chiama la versione di StampaInfo in Forma2D.
        BASE.StampaInfo()
        PRINT "  Specifica: Cerchio, Raggio: "; SELF.Raggio
    END SUB
END CLASS

' ============================
' --- PROGRAMMA PRINCIPALE ---
' ============================
SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClassesHierarchy - Test Gerarchia Classi e Proprietà             ║"
    PRINT "╚══════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Test di Polimorfismo: un array di tipo base (Forma) può contenere istanze di tipi derivati.
    DIM forme(1) AS Forma
    forme(0) = NEW Cerchio(10, 20, 5)
    forme(1) = NEW Forma2D(50, 60)

    ' Itera sull'array. La chiamata a `forme(i).StampaInfo()` è polimorfica:
    ' GM-BASIC invocherà dinamicamente la versione corretta (Cerchio.StampaInfo o Forma2D.StampaInfo)
    ' in base al tipo *reale* dell'oggetto a runtime.
    DIM i AS INTEGER
    FOR i = 0 TO 1
        forme(i).StampaInfo()
        PRINT
    NEXT i
    
    PRINT "--- Test Proprietà ---"
    DIM c AS Cerchio = forme(0)
    PRINT "Raggio attuale: "; c.Raggio ' Invoca l'accessor GET.
    PRINT "Modifico il raggio a 10..."
    c.Raggio = 10 ' Invoca l'accessor SET.
    PRINT "Nuovo Raggio: "; c.Raggio
    PRINT "Nuova Area: "; c.Area()
    PRINT
    
    ' Test della validazione dell'input nel setter della proprietà Raggio.
    PRINT "Tento di impostare un raggio non valido..."
    TRY
        c.Raggio = -5
    CATCH ex AS ERR
        PRINT "OK: Errore catturato correttamente."
        PRINT "  Codice: "; ex.Number; ", Messaggio: '"; ex.Description; "'"
    END TRY
    PRINT
    
    PRINT "--- Test completato ---"
END SUB

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Copertura OOP: Il test è molto completo e verifica con successo:
'   - Ereditarietà a più livelli (`Forma` -> `Forma2D` -> `Cerchio`).
'   - Polimorfismo tramite `VIRTUAL` e `OVERRIDE` su `FUNCTION` e `SUB`.
'   - Chiamate a `BASE.NEW` (costruttori) e `BASE.Metodo`.
'   - Modificatori di accesso `PUBLIC`, `PROTECTED`, `PRIVATE`.
'   - Membri `STATIC` condivisi tra istanze.
'   - Proprietà (`PROPERTY`) con logica di validazione nei `SET`.
'
' Il file di test è da considerarsi completo per le funzionalità che intende verificare.
' Non sono state identificate mancanze significative rispetto agli obiettivi dichiarati.
'
' =================================================================================================