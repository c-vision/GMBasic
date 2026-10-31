' FILE: samples/Community/Classes/ClassesInheritsCrossFileTransitive/Auto.inc
' =================================================================================================
'   Livello intermedio della catena: IMPORTA Mezzi, eredita Mezzi.Veicolo, fa OVERRIDE Tipo()
' =================================================================================================
' Questo è il livello 2 della catena a 3 livelli usata per il BUG-4 (Classi-Distruttore-
' Ereditarieta-CrossFile-Bugs): Veicolo (livello 1, Mezzi.inc) <- Auto (livello 2, questo file)
' <- Furgone (livello 3, il .bas principale, che fa OVERRIDE Tipo() UNA SECONDA VOLTA sopra
' l'override di Auto). Prima del fix di BUG-1/BUG-4 (ordine di binding dei moduli,
' Linker.Binding.cs, commit fde1fba6), a seconda del nome/ordine alfabetico dei file questa
' catena poteva corrompere i campi ereditati o mandare in crash la PRINT finale. Dopo il fix la
' catena funziona correttamente end-to-end.
'
' Il NAMESPACE termina a fine file (nessun END NAMESPACE).
' =================================================================================================

NAMESPACE Veicoli

IMPORT Mezzi

PUBLIC CLASS Auto INHERITS Mezzi.Veicolo
    PUBLIC DIM Porte AS LONG

    PUBLIC SUB NEW(marca AS STRING, cavalli AS LONG, porte AS LONG)
        BASE.NEW(marca, cavalli)
        SELF.Porte = porte
    END SUB

    PUBLIC OVERRIDE FUNCTION Tipo() AS STRING
        FUNCTION = "auto a " + LTRIM$(STR(SELF.Porte)) + " porte"
    END FUNCTION
END CLASS
