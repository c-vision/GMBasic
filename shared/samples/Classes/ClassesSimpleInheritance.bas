' FILE: Classes/ClassesSimpleInheritance.bas
' =================================================================================================
'        GUIDA TEORICA: Ereditarietà Semplice, Polimorfismo e Overloading degli Operatori
' =================================================================================================
'
' Questo test dimostra tre pilastri della programmazione a oggetti (OOP) in GM-BASIC:
' l'ereditarietà, il polimorfismo e l'overloading degli operatori.
'
' 1. EREDITARIETÀ (`INHERITS`)
'    L'ereditarietà permette a una classe (derivata) di acquisire i membri di un'altra classe (base).
'    Questo stabilisce una relazione "è un" (es. un `Cane` "è un" `Animale`).
'    - `INHERITS Animale`: Dichiara `Cane` come specializzazione di `Animale`.
'    - `BASE.NEW(...)`: Un costruttore di una classe derivata deve chiamare quello della base
'      per garantire la corretta inizializzazione dei membri ereditati.
'
' 2. POLIMORFISMO (`VIRTUAL` e `OVERRIDE`)
'    Il polimorfismo ("molte forme") è la capacità di trattare oggetti di classi diverse attraverso
'    un'interfaccia comune. In questo test, un array di `Animale` contiene oggetti `Cane` e `Gatto`.
'    - `VIRTUAL`: Un metodo dichiarato `VIRTUAL` nella classe base può essere ridefinito.
'    - `OVERRIDE`: Un metodo `OVERRIDE` fornisce una nuova implementazione per un metodo `VIRTUAL`.
'    Quando `a.MakeSound()` viene chiamato, GM-BASIC esegue la versione corretta del metodo
'    in base al tipo *reale* dell'oggetto a runtime.
'
' 3. OVERLOADING DEGLI OPERATORI (`OPERATOR`)
'    Questa funzionalità permette di definire un comportamento personalizzato per gli operatori
'    standard (`+`, `-`, `=`, `<>`) quando usati con le proprie classi.
'    - Operatori Binari (`+`, `=`): `PUBLIC STATIC OPERATOR +(a AS T, b AS T) AS T`
'    - Operatori Unari (`-`): `PUBLIC STATIC OPERATOR -(a AS T) AS T`
'    Gli operatori sono tipicamente `STATIC` perché operano sui loro argomenti.
'    La keyword `OPERATOR` viene usata per assegnare il valore di ritorno, come in una `FUNCTION`.
'
' =================================================================================================

' --- CLASSE BASE ---
PUBLIC CLASS Animale
    PUBLIC DIM Nome AS STRING * 20
    
    PUBLIC SUB NEW(nomeIniziale AS STRING)
        SELF.Nome = nomeIniziale
    END SUB

    PUBLIC VIRTUAL SUB MakeSound()
        PRINT SELF.Nome; " fa un suono generico..."
    END SUB
END CLASS

' --- CLASSE DERIVATA 1 ---
PUBLIC CLASS Cane INHERITS Animale
    PUBLIC SUB NEW(nomeIniziale AS STRING)
        BASE.NEW(nomeIniziale)
    END SUB

    PUBLIC OVERRIDE SUB MakeSound()
        PRINT SELF.Nome; " fa: Bau!"
    END SUB
END CLASS

' --- CLASSE DERIVATA 2 ---
PUBLIC CLASS Gatto INHERITS Animale
    PUBLIC SUB NEW(nomeIniziale AS STRING)
        BASE.NEW(nomeIniziale)
    END SUB

    PUBLIC OVERRIDE SUB MakeSound()
        PRINT SELF.Nome; " fa: Miao!"
    END SUB
END CLASS

' --- CLASSE PER TESTARE GLI OPERATORI BINARI ---
PUBLIC CLASS Mandria
    PUBLIC DIM Animali AS LIST OF Animale
    
    PUBLIC SUB NEW()
        SELF.Animali = NEW LIST OF Animale
    END SUB
    
    ' Operatore sovraccaricato per unire due mandrie
    PUBLIC STATIC OPERATOR +(a AS Mandria, b AS Mandria) AS Mandria
        DIM nuovaMandria AS Mandria = NEW Mandria()
        nuovaMandria.Animali.AddRange(a.Animali)
        nuovaMandria.Animali.AddRange(b.Animali)
        OPERATOR = nuovaMandria
    END OPERATOR
    
    PUBLIC SUB StampaMembri()
        PRINT "La mandria contiene "; SELF.Animali.Count(); " animali:"
        FOR EACH a AS Animale IN SELF.Animali
            PRINT "  - "; a.Nome
        NEXT
    END SUB
END CLASS

' --- NUOVA CLASSE PER TESTARE OPERATORI UNARI E DI CONFRONTO ---
PUBLIC CLASS Punto
    PUBLIC DIM X AS DOUBLE, Y AS DOUBLE
    
    PUBLIC SUB NEW(x_val AS DOUBLE, y_val AS DOUBLE)
        SELF.X = x_val
        SELF.Y = y_val
    END SUB
    
    ' Operatore unario di negazione
    PUBLIC STATIC OPERATOR -(a AS Punto) AS Punto
        OPERATOR = NEW Punto(-a.X, -a.Y)
    END OPERATOR
    
    ' Operatore di confronto per uguaglianza
    PUBLIC STATIC OPERATOR =(a AS Punto, b AS Punto) AS BIT
        OPERATOR = (a.X = b.X AND a.Y = b.Y)
    END OPERATOR
    
    ' Operatore di confronto per disuguaglianza
    PUBLIC STATIC OPERATOR <>(a AS Punto, b AS Punto) AS BIT
        OPERATOR = NOT (a = b) ' Riutilizza l'operatore di uguaglianza
    END OPERATOR
END CLASS


' --- PROGRAMMA PRINCIPALE ---
SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClassesSimpleInheritance - Test Ereditarietà e Polimorfismo    ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT

    DIM animali(2) AS Animale
    animali(0) = NEW Animale("Generico")
    animali(1) = NEW Cane("Fido")
    animali(2) = NEW Gatto("Silvestro")
    
    PRINT "--- 1. Dimostrazione Polimorfismo ---"
    DIM i AS INTEGER
    FOR i = 0 TO 2
        animali(i).MakeSound()
    NEXT i
    PRINT
    
    PRINT "--- 2. Test Overloading Operatore Binario '+' ---"
    DIM mandriaCani AS Mandria = NEW Mandria()
    mandriaCani.Animali.Add(NEW Cane("Pluto"))
    mandriaCani.Animali.Add(NEW Cane("Lassie"))
    
    DIM mandriaGatti AS Mandria = NEW Mandria()
    mandriaGatti.Animali.Add(NEW Gatto("Tom"))
    
    PRINT "Unisco le due mandrie..."
    DIM mandriaMista AS Mandria = mandriaCani + mandriaGatti
    mandriaMista.StampaMembri()
    PRINT
    
    PRINT "--- 3. Test Operatori Unari e di Confronto ---"
    DIM p1 AS Punto = NEW Punto(10, 20)
    DIM p2 AS Punto = -p1 ' Test dell'operatore unario '-'
    DIM p3 AS Punto = NEW Punto(10, 20)
    
    PRINT "Punto p1: ("; p1.X; ", "; p1.Y; ")"
    PRINT "Punto p2 (-p1): ("; p2.X; ", "; p2.Y; ")"
    PRINT "Punto p3: ("; p3.X; ", "; p3.Y; ")"
    
    ' Test degli operatori di confronto
    IF p1 = p3 THEN PRINT "  OK: p1 è uguale a p3."
    IF p1 <> p2 THEN PRINT "  OK: p1 è diverso da p2."
    PRINT
    
    PRINT "--- Test completato ---"
END SUB

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Copertura Ereditarietà e Polimorfismo: Completa e corretta.
'
' - Copertura Overloading Operatori: La copertura è stata estesa e ora include:
'   - Operatore binario: `+` (verificato)
'   - Operatore unario: `-` (verificato)
'   - Operatori di confronto: `=`, `<>` (verificati)
'
' Il file di test è da considerarsi completo per le funzionalità che intende verificare.
'
' =================================================================================================