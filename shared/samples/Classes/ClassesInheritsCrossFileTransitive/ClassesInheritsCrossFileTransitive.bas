' FILE: samples/Community/Classes/ClassesInheritsCrossFileTransitive/ClassesInheritsCrossFileTransitive.bas
' =================================================================================================
'   GUIDA TEORICA: OVERRIDE transitivo su 2+ livelli, ciascuno in un file diverso (ex BUG-4)
' =================================================================================================
' Questo sample riproduce lo scenario descritto in BUG-4 di
' Bugs/Classi-Distruttore-Ereditarieta-CrossFile-Bugs (open).md: una catena di ereditarietà a
' 3 livelli, con la BASE importata (Mezzi.Veicolo, in Mezzi.inc) e un metodo VIRTUAL
' ri-overrideato DUE VOLTE lungo la catena, ciascun livello in un file .inc/.bas separato:
'
'   Mezzi.Veicolo        (Mezzi.inc)   VIRTUAL  Tipo() = "veicolo"
'   Veicoli.Auto         (Auto.inc)    OVERRIDE Tipo() = "auto a N porte"      (livello 2)
'   Furgone              (questo file) OVERRIDE Tipo() = "furgone Q q.li"      (livello 3)
'
' BUG-4 descriveva un crash PCodeVM ("The input string ' / ' was not in a correct format",
' OP_PRINT_LINE) proprio su questa forma di catena. Durante l'indagine (2026-09-02) diverse
' varianti di riproduzione (catena in un solo file, catena su 3 file separati con NAMESPACE
' annidati come qui, 3 classi SORELLE che overrideano la stessa base) sono state provate sia
' sul codice pre-fix che su quello corrente: NESSUNA variante ha piu' riprodotto il crash — solo
' la corruzione dei campi ereditati già nota come BUG-1 (campi vuoti se il file che eredita
' ordina alfabeticamente prima del file che dichiara la base), risolta dallo stesso fix
' (Linker.Binding.cs, OrderModulesForBinding, commit fde1fba6). Questo sample dimostra quindi il
' caso FUNZIONANTE dopo il fix: la catena a 2 override cross-file risolve il dispatch
' correttamente e nessun campo risulta corrotto, qualunque sia l'ordine alfabetico dei file
' (Auto.inc < ClassesInheritsCrossFileTransitive.bas < Mezzi.inc).
' =================================================================================================

IMPORT Veicoli
IMPORT Mezzi

' --- Livello 3 della catena: eredita da Auto (livello 2, importato), OVERRIDE Tipo() di nuovo ---
PUBLIC CLASS Furgone INHERITS Veicoli.Auto
    PUBLIC DIM Quintali AS LONG

    PUBLIC SUB NEW(marca AS STRING, cavalli AS LONG, quintali AS LONG)
        BASE.NEW(marca, cavalli, 2)    ' un furgone ha tipicamente 2 porte
        SELF.Quintali = quintali
    END SUB

    PUBLIC OVERRIDE FUNCTION Tipo() AS STRING
        FUNCTION = "furgone " + LTRIM$(STR(SELF.Quintali)) + " q.li"
    END FUNCTION
END CLASS

SUB Main()
    PRINT "== Classi — OVERRIDE transitivo su 2 livelli, cross-file (ex BUG-4) =="
    PRINT

    DIM a AS Veicoli.Auto = NEW Veicoli.Auto("Fiat", 90, 5)
    DIM f AS Furgone = NEW Furgone("Ducati", 120, 35)

    PRINT ">> Livello 2 (Auto, override singolo)"
    PRINT "   "; a.GetMarca(); " "; a.Cavalli; "cv -> "; a.Tipo()
    PRINT

    PRINT ">> Livello 3 (Furgone, override del override — la catena transitiva di BUG-4)"
    PRINT "   "; f.GetMarca(); " "; f.Cavalli; "cv -> "; f.Tipo()
    PRINT

    ' Polimorfismo: la funzione accetta la BASE (livello 1, importata) e invoca il VIRTUAL.
    ' Il dispatch deve risolvere Tipo() sull'OVERRIDE più derivato (Furgone), non su quello
    ' intermedio (Auto) né sulla base (Veicolo) — questo è esattamente il punto che BUG-4
    ' segnalava come rotto.
    PRINT ">> Polimorfismo: dispatch sull'override più derivato tramite la base importata"
    PRINT "   "; Descrivi(a)
    PRINT "   "; Descrivi(f)
END SUB

FUNCTION Descrivi(v AS Mezzi.Veicolo) AS STRING
    Descrivi = v.GetMarca() + " -> " + v.Tipo()
END FUNCTION
