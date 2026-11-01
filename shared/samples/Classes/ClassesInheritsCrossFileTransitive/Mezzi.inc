' FILE: samples/Classes/ClassesInheritsCrossFileTransitive/Mezzi.inc
' =================================================================================================
'   Classe base in un modulo separato — bersaglio del BUG-4 (override transitivo cross-file)
' =================================================================================================
' Stessa base di ClassesInheritsCrossFile/Mezzi.inc: vive in un NAMESPACE, in un file separato,
' con un metodo VIRTUAL da overrideare a catena da PIÙ file diversi (vedi Auto.inc e il .bas
' principale in questa cartella).
'
' Il NAMESPACE termina a fine file (nessun END NAMESPACE).
' =================================================================================================

NAMESPACE Mezzi

PUBLIC CLASS Veicolo
    PUBLIC DIM Marca AS STRING
    PUBLIC DIM Cavalli AS LONG

    PUBLIC SUB NEW(marca AS STRING, cavalli AS LONG)
        SELF.Marca = marca
        SELF.Cavalli = cavalli
    END SUB

    PUBLIC FUNCTION GetMarca() AS STRING
        FUNCTION = SELF.Marca
    END FUNCTION

    PUBLIC VIRTUAL FUNCTION Tipo() AS STRING
        FUNCTION = "veicolo"
    END FUNCTION
END CLASS
