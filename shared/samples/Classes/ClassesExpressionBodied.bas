' FILE: Classes/ClassesExpressionBodied.bas
' =================================================================================================
'               GUIDA TEORICA: SINTASSI A CORPO DI ESPRESSIONE (=>) E OOP AVANZATO
' =================================================================================================
'
' Questo test è una dimostrazione completa della sintassi a "corpo di espressione" (`=>`),
' una funzionalità moderna che permette di definire membri di classe in modo più conciso.
' Dimostra anche concetti chiave della programmazione a oggetti (OOP) come l'ereditarietà,
' il polimorfismo e i modificatori di accesso.
'
' 1. SINTASSI A CORPO DI ESPRESSIONE (`=>`)
'    La "fat arrow" (`=>`) fornisce una scorciatoia per definire membri che consistono in una
'    singola espressione o una singola istruzione.
'
'    - `FUNCTION ... => espressione`:
'      Equivale a un blocco `FUNCTION` che contiene solo `RETURN espressione`.
'      Esempio: `FUNCTION Area() AS DOUBLE => 0`
'
'    - `SUB ... => istruzione`:
'      Equivale a un blocco `SUB` che contiene solo quella singola istruzione.
'      Esempio: `SUB Stampa() => PRINT "Ciao"`
'
'    - `GET => espressione`:
'      In una proprietà, equivale a un blocco `GET` che contiene `RETURN espressione`.
'      Esempio: `PROPERTY ID AS LONG GET => _id`
'
'    - `SET(...) => istruzione`:
'      In una proprietà, equivale a un blocco `SET` con una singola istruzione.
'      Esempio: `SET(v AS DOUBLE) => _raggio = v`
'
'    - `SUB NEW(...) => BASE.NEW(...)`:
'      Un caso speciale per i costruttori. Se l'unica operazione è chiamare il costruttore
'      della classe base, si può usare la sintassi a corpo di espressione.
'
' 2. EREDITARIETÀ (`INHERITS`) E GERARCHIA
'    Una classe può ereditare campi e metodi da un'altra (la classe base) usando la
'    keyword `INHERITS`. Questo test dimostra una gerarchia a più livelli:
'    `Forma` -> `Forma2D` -> `Cerchio`.
'
' 3. POLIMORFISMO (`VIRTUAL` e `OVERRIDE`)
'    - `VIRTUAL`: Un metodo dichiarato `VIRTUAL` nella classe base può essere ridefinito
'      (sovrascritto) nelle classi derivate.
'    - `OVERRIDE`: Un metodo dichiarato `OVERRIDE` in una classe derivata fornisce una
'      nuova implementazione per un metodo `VIRTUAL` della classe base.
'    Quando un metodo virtuale viene chiamato su un oggetto, GM-BASIC esegue la versione
'    del metodo che corrisponde al tipo *reale* dell'oggetto a runtime, non al tipo della
'    variabile che lo contiene. Questo è il cuore del polimorfismo.
'
' 4. ACCESSO ALLA CLASSE BASE (`BASE`)
'    - `BASE.NEW(...)`: Chiama il costruttore della classe base. È obbligatorio farlo
'      come prima istruzione in un costruttore di una classe derivata se la base
'      non ha un costruttore senza parametri.
'    - `BASE.<Metodo>(...)`: Chiama l'implementazione di un metodo della classe base.
'
' 5. MEMBRI STATICI (`STATIC`)
'    Un membro `STATIC` appartiene alla classe stessa, non a una singola istanza.
'    Tutte le istanze della classe condividono lo stesso membro statico. In questo
'    esempio, `_nextId` è un contatore globale per tutte le `Forma`.
'
' =================================================================================================


' --- CLASSE BASE: Definisce le proprietà e i comportamenti comuni di una forma geometrica. ---
PUBLIC CLASS Forma
    PROTECTED DIM X AS DOUBLE ' Visibile a questa classe e alle classi derivate
    PROTECTED DIM Y AS DOUBLE
    PRIVATE DIM _id AS LONG   ' Visibile solo all'interno della classe Forma
    PUBLIC STATIC DIM _nextId AS LONG = 1 ' Condiviso tra tutte le istanze di Forma

    ' Costruttore con corpo a blocco, usato per eseguire operazioni multiple.
    PUBLIC SUB NEW(x_pos AS DOUBLE, y_pos AS DOUBLE)
        SELF.X = x_pos
        SELF.Y = y_pos
        SELF._id = Forma._nextId ' Assegna l'ID univoco e incrementa il contatore statico
        Forma._nextId = Forma._nextId + 1
    END SUB
    
    ' Proprietà di sola lettura con sintassi a corpo di espressione (`=>`).
    ' Equivale a: GET RETURN SELF._id END GET
    PUBLIC PROPERTY ID AS LONG => SELF._id

    ' Metodo VIRTUAL con corpo a espressione. Le classi derivate possono fornire
    ' una loro implementazione. Qui restituisce un valore di default.
    PUBLIC VIRTUAL FUNCTION Area() AS DOUBLE => 0

    ' SUB VIRTUAL con corpo a espressione. Esegue una singola istruzione.
    PUBLIC VIRTUAL SUB StampaInfo() => PRINT "Forma ID "; SELF.ID; " at ("; SELF.X; ", "; SELF.Y; ")"
END CLASS


' --- CLASSE DERIVATA (LIVELLO 1): Specializza Forma per il 2D. ---
PUBLIC CLASS Forma2D INHERITS Forma
    ' Costruttore con corpo a espressione che chiama il costruttore della classe base.
    ' Questa è la forma più concisa per i costruttori che delegano solo l'inizializzazione.
    PUBLIC SUB NEW(x_pos AS DOUBLE, y_pos AS DOUBLE) => BASE.NEW(x_pos, y_pos)

    ' Override del metodo StampaInfo. Utilizza un corpo a blocco perché esegue più istruzioni.
    PUBLIC OVERRIDE SUB StampaInfo()
        ' Chiama l'implementazione del metodo nella classe base (Forma).
        BASE.StampaInfo()
        ' Aggiunge informazioni specifiche della classe derivata.
        PRINT "  Tipo: Forma 2D, Area: "; SELF.Area()
    END SUB
END CLASS


' --- CLASSE DERIVATA (LIVELLO 2): Implementa una forma specifica (Cerchio). ---
PUBLIC CLASS Cerchio INHERITS Forma2D
    PRIVATE DIM _raggio AS DOUBLE

    ' Costruttore con corpo a blocco. Chiama il costruttore base e poi inizializza i suoi campi.
    PUBLIC SUB NEW(x_pos AS DOUBLE, y_pos AS DOUBLE, raggio_val AS DOUBLE)
        BASE.NEW(x_pos, y_pos) ' Chiama il costruttore di Forma2D
        SELF.Raggio = raggio_val
    END SUB

    ' Proprietà di lettura/scrittura con accessori a corpo di espressione.
    PUBLIC PROPERTY Raggio AS DOUBLE
        ' Accessor GET: restituisce il valore del campo privato.
        GET => SELF._raggio
        ' Accessor SET: esegue un'istruzione per impostare il valore, con validazione.
        SET(BYVAL value AS DOUBLE) => IF value > 0 THEN SELF._raggio = value ELSE ERROR 1000, "Il raggio deve essere positivo."
    END PROPERTY

    ' Override della funzione Area. Fornisce l'implementazione specifica per il cerchio.
    ' Utilizza una funzione dalla libreria MATH importata.
    PUBLIC OVERRIDE FUNCTION Area() AS DOUBLE => MATH.PI * SELF._raggio ^ 2
    
    ' Override del metodo StampaInfo per aggiungere i dettagli del cerchio.
    PUBLIC OVERRIDE SUB StampaInfo()
        ' Chiama la versione del metodo in Forma2D, che a sua volta chiamerà quella in Forma.
        BASE.StampaInfo()
        PRINT "  Specifica: Cerchio, Raggio: "; SELF.Raggio
    END SUB
END CLASS


' ============================
' --- PROGRAMMA PRINCIPALE ---
' ============================
SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClassesExpressionBodied - Test Sintassi a Corpo di Espressione (=>)     ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Test di Polimorfismo: un array di tipo base (Forma) può contenere istanze di tipi derivati.
    DIM forme(1) AS Forma
    forme(0) = NEW Cerchio(10, 20, 5)
    forme(1) = NEW Forma2D(50, 60)

    ' Itera sull'array e chiama il metodo virtuale StampaInfo().
    ' GM-BASIC invocherà dinamicamente la versione corretta (Cerchio.StampaInfo o Forma2D.StampaInfo)
    ' in base al tipo reale dell'oggetto a runtime.
    DIM i AS INTEGER
    FOR i = 0 TO 1
        forme(i).StampaInfo()
        PRINT
    NEXT i
    
    ' Test specifico degli accessori della proprietà Raggio.
    PRINT "--- Test Proprietà ---"
    DIM c AS Cerchio = forme(0)
    PRINT "Raggio attuale: "; c.Raggio
    PRINT "Modifico il raggio a 10..."
    c.Raggio = 10 ' Invoca l'accessor SET
    PRINT "Nuovo Raggio: "; c.Raggio ' Invoca l'accessor GET
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
' - La copertura per la sintassi a corpo di espressione (`=>`) è eccellente e copre:
'   - FUNCTION (virtuale e override)
'   - SUB (virtuale)
'   - Costruttore (chiamata a BASE.NEW)
'   - Proprietà (sia GET che SET)
'
' - La copertura dei concetti OOP è molto buona:
'   - Ereditarietà a più livelli.
'   - Polimorfismo con `VIRTUAL` e `OVERRIDE`.
'   - Chiamate a `BASE` sia per costruttori che per metodi.
'   - Modificatori di accesso `PUBLIC`, `PROTECTED`, `PRIVATE` e membro `STATIC`.
'
' Il file di test è da considerarsi completo per le funzionalità che intende verificare.
'
' =================================================================================================