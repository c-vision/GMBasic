' FILE: samples/Community/Classes/ClassesInheritsCrossFile/Mezzi.inc
' =================================================================================================
'   Classe base in un modulo separato — fix Z3 (INHERITS qualificato / cross-file)
' =================================================================================================
' La classe base vive in questo file .inc dentro un NAMESPACE. Il programma che la importa
' può dichiarare `INHERITS Mezzi.Veicolo` (base QUALIFICATA, da un altro file/modulo).
'
' NB: NON usare il namespace "Vehicles" (collide con un identificatore interno e i metodi
' della base che leggono i campi restituiscono vuoto a runtime — vedi Bugs/Classi-Bug-Report).
'
' Il NAMESPACE termina a fine file (nessun END NAMESPACE).
' =================================================================================================

NAMESPACE Mezzi

PUBLIC CLASS Veicolo
    PUBLIC DIM Marca AS STRING
    PUBLIC DIM Velocita AS LONG = 0
    PRIVATE DIM _chilometri AS LONG

    PUBLIC SUB NEW(marca AS STRING)
        SELF.Marca = marca
        SELF._chilometri = 0
    END SUB

    PUBLIC SUB Accelera(delta AS LONG)
        SELF.Velocita = SELF.Velocita + delta
        SELF._chilometri = SELF._chilometri + delta
    END SUB

    ' Accessor per lo stato della base: i campi ereditati ORA sono leggibili/scrivibili da
    ' un'istanza esterna (LIMITE-9 risolto, commit 2cfed7ae), ma gli accessor restano la
    ' buona pratica per incapsulare i campi (e leggere campi PRIVATE).
    PUBLIC FUNCTION GetMarca() AS STRING
        FUNCTION = SELF.Marca
    END FUNCTION

    PUBLIC FUNCTION GetVelocita() AS LONG
        FUNCTION = SELF.Velocita
    END FUNCTION

    PUBLIC FUNCTION Chilometri() AS LONG
        FUNCTION = SELF._chilometri
    END FUNCTION

    PUBLIC VIRTUAL FUNCTION Tipo() AS STRING
        FUNCTION = "veicolo"
    END FUNCTION

    PUBLIC SUB DESTRUCTOR
        PRINT "  [veicolo demolito]"
    END SUB
END CLASS
