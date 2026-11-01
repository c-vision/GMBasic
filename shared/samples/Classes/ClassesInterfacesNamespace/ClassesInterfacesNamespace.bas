' FILE: samples/Classes/ClassesInterfacesNamespace/ClassesInterfacesNamespace.bas
' =================================================================================================
'   GUIDA TEORICA: INTERFACE esportate da un NAMESPACE (fix Z2) + IMPLEMENTS multipli
' =================================================================================================
' Un'INTERFACE è un contratto: dichiara le firme che una classe si impegna a fornire.
' Con il fix Z2 le `PUBLIC INTERFACE` dentro un NAMESPACE vengono ESPORTATE: il programma
' le importa con `IMPORT` e una classe può aderire a più contratti con `IMPLEMENTS`.
'
' Vantaggi pratici:
'   - il codice generico può accettare "qualsiasi oggetto che implementi il contratto";
'   - più classi, anche non imparentate, diventano intercambiabili attraverso l'interfaccia;
'   - le firme sono dichiarate UNA volta e verificate dal linker (nessuna compatibilità testuale).
'
' Questo sample implementa i due contratti di Contracts.inc (ICalcolabile, IDisegnabile)
' in classi NON imparentate (Quadrato, Cerchio) e le usa attraverso l'interfaccia.
' =================================================================================================

IMPORT Shapes

' --- Quadrato: implementa ENTRAMBI i contratti ---
PUBLIC CLASS Quadrato IMPLEMENTS Shapes.ICalcolabile, Shapes.IDisegnabile
    PUBLIC DIM Lato AS DOUBLE

    PUBLIC SUB NEW(lato AS DOUBLE)
        SELF.Lato = lato
    END SUB

    PUBLIC FUNCTION Area() AS DOUBLE
        FUNCTION = SELF.Lato * SELF.Lato
    END FUNCTION

    PUBLIC FUNCTION Etichetta() AS STRING
        FUNCTION = "Quadrato (lato " + LTRIM$(STR(SELF.Lato)) + ")"
    END FUNCTION

    PUBLIC SUB Disegna()
        PRINT "   [####]"
    END SUB
END CLASS

' --- Cerchio: implementa ENTRAMBI i contratti ---
PUBLIC CLASS Cerchio IMPLEMENTS Shapes.ICalcolabile, Shapes.IDisegnabile
    PUBLIC DIM Raggio AS DOUBLE

    PUBLIC SUB NEW(raggio AS DOUBLE)
        SELF.Raggio = raggio
    END SUB

    PUBLIC FUNCTION Area() AS DOUBLE
        FUNCTION = 3.14159 * SELF.Raggio * SELF.Raggio
    END FUNCTION

    PUBLIC FUNCTION Etichetta() AS STRING
        FUNCTION = "Cerchio (raggio " + LTRIM$(STR(SELF.Raggio)) + ")"
    END FUNCTION

    PUBLIC SUB Disegna()
        PRINT "   (o)"
    END SUB
END CLASS

SUB Main()
    PRINT "== Classi — INTERFACE in NAMESPACE (Z2) + IMPLEMENTS multipli =="
    PRINT

    ' --- Uso attraverso il contratto ICalcolabile (aree + etichette) ---
    PRINT ">> Attraverso Shapes.ICalcolabile"
    DIM forme(1) AS Shapes.ICalcolabile
    forme(0) = NEW Quadrato(4)
    forme(1) = NEW Cerchio(2)

    DIM i AS LONG
    FOR i = 0 TO 1
        PRINT "   "; forme(i).Etichetta(); " -> area "; forme(i).Area()
    NEXT i
    PRINT

    ' --- Uso attraverso il contratto IDisegnabile (disegno) ---
    PRINT ">> Attraverso Shapes.IDisegnabile"
    DIM disegni(1) AS Shapes.IDisegnabile
    disegni(0) = NEW Quadrato(3)
    disegni(1) = NEW Cerchio(1)

    FOR i = 0 TO 1
        disegni(i).Disegna()
    NEXT i
    PRINT

    ' --- Riepilogo: la stessa classe soddisfa entrambi i contratti ---
    PRINT ">> Una stessa istanza usata con entrambi i contratti"
    ' (ogni variabile si ottiene dalla istanza CONCRETA, non da un'altra interfaccia)
    DIM qObj AS Quadrato = NEW Quadrato(5)
    DIM q AS Shapes.ICalcolabile = qObj
    DIM qd AS Shapes.IDisegnabile = qObj
    PRINT "   "; q.Etichetta(); " area "; q.Area()
    qd.Disegna()
END SUB
