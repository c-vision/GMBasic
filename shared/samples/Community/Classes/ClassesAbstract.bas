' FILE: Classes/ClassesAbstract.bas
' =================================================================================================
'        GUIDA TEORICA: Classi e Metodi Astratti (ABSTRACT)
' =================================================================================================
'
' Questo file di test è progettato per verificare l'implementazione delle classi e dei metodi astratti
' in GM-BASIC, una funzionalità fondamentale per la programmazione orientata agli oggetti che promuove
' un design robusto e contratti di interfaccia chiari.
'
' 1. CLASSE ASTRATTA (`ABSTRACT CLASS`)
'    Una classe dichiarata come `ABSTRACT` funge da modello per altre classi, ma non può essere
'    istanziata direttamente. Tenta di creare un'istanza con `NEW` deve generare un errore di compilazione.
'    - Contratto: Definisce un'interfaccia comune per tutte le classi derivate.
'    - Membri: Può contenere sia metodi astratti (senza corpo) che metodi concreti (con implementazione).
'
' 2. METODO ASTRATTO (`ABSTRACT METHOD`)
'    Un metodo dichiarato come `ABSTRACT` all'interno di una classe astratta definisce una firma
'    ma non fornisce un'implementazione (non ha corpo e termina con un newline).
'    - Obbligo di Implementazione: Qualsiasi classe concreta che eredita dalla classe astratta
'      DEVE fornire un'implementazione per tutti i metodi astratti ereditati, usando la keyword `OVERRIDE`.
'    - Implicita Virtualità: Un metodo astratto è implicitamente `VIRTUAL`.
'    - Vincoli: I metodi astratti non possono essere `PRIVATE`.
'
' OBIETTIVI DI QUESTO TEST:
' 1. Validare che una `ABSTRACT CLASS` non possa essere istanziata (vedi `Main`).
' 2. Validare che una classe concreta che eredita da una astratta DEVE implementare TUTTI i metodi astratti.
'    (vedi la classe `Triangolo` commentata).
' 3. Verificare il corretto funzionamento del polimorfismo, dove un array del tipo base astratto
'    può contenere istanze di tipi derivati concreti, e le chiamate ai metodi invocano l'implementazione
'    corretta (`OVERRIDE`).
' 4. Verificare che una classe astratta possa contenere metodi concreti che vengono ereditati normalmente.
'
' =================================================================================================

' --- CLASSE BASE ASTRATTA ---
' Questa classe non può essere istanziata. Definisce il "contratto" per tutte le figure.
PUBLIC ABSTRACT CLASS FiguraGeometrica
    PUBLIC DIM Nome AS STRING * 30
    
    PUBLIC SUB NEW(nomeIniziale AS STRING)
        SELF.Nome = nomeIniziale
    END SUB

    ' Metodo concreto: viene ereditato e può essere usato da tutte le classi figlie.
    PUBLIC SUB MostraNome()
        PRINT "Questa figura è un/una: "; SELF.Nome
    END SUB
    
    ' Metodo astratto: DEVE essere implementato dalle classi derivate. Non ha corpo.
    PUBLIC ABSTRACT FUNCTION CalcolaArea() AS DOUBLE
    
    ' Altro metodo astratto.
    PUBLIC ABSTRACT SUB Disegna()
END CLASS


' --- CLASSE CONCRETA 1 ---
' Implementa tutti i membri astratti di FiguraGeometrica.
PUBLIC CLASS Cerchio INHERITS FiguraGeometrica
    PUBLIC DIM Raggio AS DOUBLE

    PUBLIC SUB NEW(nome AS STRING, raggioCerchio AS DOUBLE)
        BASE.NEW(nome)
        SELF.Raggio = raggioCerchio
    END SUB
    
    ' Implementazione obbligatoria del metodo astratto.
    PUBLIC OVERRIDE FUNCTION CalcolaArea() AS DOUBLE
        FUNCTION = 3.14159 * SELF.Raggio ^ 2
    END FUNCTION
    
    ' Implementazione obbligatoria del metodo astratto.
    PUBLIC OVERRIDE SUB Disegna()
        PRINT "Disegno un cerchio di raggio "; SELF.Raggio
    END SUB
END CLASS


' --- CLASSE CONCRETA 2 ---
' Implementa tutti i membri astratti di FiguraGeometrica.
PUBLIC CLASS Rettangolo INHERITS FiguraGeometrica
    PUBLIC DIM Larghezza AS DOUBLE, Altezza AS DOUBLE

    PUBLIC SUB NEW(nome AS STRING, w AS DOUBLE, h AS DOUBLE)
        BASE.NEW(nome)
        SELF.Larghezza = w
        SELF.Altezza = h
    END SUB
    
    ' Implementazione obbligatoria.
    PUBLIC OVERRIDE FUNCTION CalcolaArea() AS DOUBLE
        FUNCTION = SELF.Larghezza * SELF.Altezza
    END FUNCTION
    
    ' Implementazione obbligatoria.
    PUBLIC OVERRIDE SUB Disegna()
        PRINT "Disegno un rettangolo di dimensioni "; SELF.Larghezza; "x"; SELF.Altezza
    END SUB
END CLASS


' --- CLASSE CONCRETA 3 (INTENZIONALMENTE INCOMPLETA E COMMENTATA) ---
' Se de-commentata, questa classe DEVE generare un errore di compilazione perché
' non implementa il metodo astratto `Disegna()` ereditato da `FiguraGeometrica`.
'
' PUBLIC CLASS Triangolo INHERITS FiguraGeometrica
'     PUBLIC DIM Base AS DOUBLE, Altezza AS DOUBLE
'
'     PUBLIC SUB NEW(nome AS STRING, b AS DOUBLE, h AS DOUBLE)
'         BASE.NEW(nome)
'         SELF.Base = b
'         SELF.Altezza = h
'     END SUB
'
'     PUBLIC OVERRIDE FUNCTION CalcolaArea() AS DOUBLE
'         FUNCTION = (SELF.Base * SELF.Altezza) / 2
'     END FUNCTION
'
'     ' ERRORE: Manca l'implementazione di "OVERRIDE SUB Disegna()"
' END CLASS
'


' --- PROGRAMMA PRINCIPALE ---
SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClassesAbstract - Test Classi e Metodi Astratti                ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- TEST 1: Verifica del Polimorfismo ---
    PRINT "--- 1. Test Polimorfismo su Array di tipo Astratto ---"
    
    ' È possibile creare un array del tipo astratto, ma conterrà solo istanze di tipi concreti.
    DIM figure(1) AS FiguraGeometrica
    figure(0) = NEW Cerchio("Cerchio Grande", 10)
    figure(1) = NEW Rettangolo("Rettangolo Blu", 5, 8)

    FOR EACH fig AS FiguraGeometrica IN figure
        fig.MostraNome()      ' Chiama metodo concreto ereditato
        fig.Disegna()         ' Chiama metodo astratto implementato (OVERRIDE)
        PRINT "   Area: "; fig.CalcolaArea() ' Chiama metodo astratto implementato (OVERRIDE)
        PRINT
    NEXT
    
    
    ' --- TEST 2: Verifica del Blocco di Istanziazione (DEVE FALLIRE) ---
    PRINT "--- 2. Test Blocco Istanziazione Classe Astratta (commentato) ---"
    PRINT "   (Se de-commentata, la riga seguente deve causare un errore di compilazione)"
    '
    ' ERRORE DI COMPILAZIONE ATTESO: Impossibile creare un'istanza della classe astratta 'FiguraGeometrica'.
    ' DIM figAstratta AS FiguraGeometrica = NEW FiguraGeometrica("Forma Sconosciuta")
    '
    PRINT "   OK: Blocco di test superato (se compilato)."
    PRINT
    
    PRINT "--- Test completato ---"
END SUB
