' FILE: samples/Classes/ClassesInheritsCrossFile/ClassesInheritsCrossFile.bas
' =================================================================================================
'   GUIDA TEORICA: INHERITS qualificato / da un altro file (fix Z3)
' =================================================================================================
' Prima del fix Z3 la classe base doveva stare nello STESSO file di parsing e con nome nudo:
' `INHERITS Veicolo`. Se la base viveva in un modulo .inc (sottocartella) o era qualificata
' (`INHERITS Mezzi.Veicolo`) il parse falliva.
'
' Dopo il fix Z3:
'   - `INHERITS`/`IMPLEMENTS` accettano un nome QUALIFICATO (`Ns.Classe`);
'   - la base può essere definita in un ALTRO file/modulo importato con `IMPORT`;
'   - il linker risolve la base transitivamente per SymbolId (mai per testo).
'
' Questo sample dimostra:
'   1. INHERITS qualificato/cross-file: `Auto INHERITS Mezzi.Veicolo`;
'   2. `BASE.NEW(...)` per chiamare il costruttore della base da un altro file;
'   3. campi/metodi ereditati acceduti tramite METODI della base (GetMarca/GetVelocita);
'   4. polimorfismo VIRTUAL/OVERRIDE con la base importata (variabili dirette E array).
'
' LIMITI NOTI (vedi Bugs/Classi-Bug-Report.md): con PIÙ di 2 derivate dalla base importata
' (specie con una gerarchia transitiva) il PCodeVM corrompe l'output; un OVERRIDE su più
' livelli della catena cross-file fallisce a runtime. Questo sample resta entro i limiti.
' =================================================================================================

IMPORT Mezzi

' --- Derivata 1: dalla base IMPORTATA e qualificata ---
PUBLIC CLASS Auto INHERITS Mezzi.Veicolo
    PUBLIC DIM Porte AS LONG

    PUBLIC SUB NEW(marca AS STRING, porte AS LONG)
        BASE.NEW(marca)                ' costruttore della base (altro file)
        SELF.Porte = porte
    END SUB

    PUBLIC OVERRIDE FUNCTION Tipo() AS STRING
        FUNCTION = "auto a " + LTRIM$(STR(SELF.Porte)) + " porte"
    END FUNCTION
END CLASS

' --- Derivata 2: dalla stessa base importata ---
PUBLIC CLASS Moto INHERITS Mezzi.Veicolo
    PUBLIC DIM Cilindrata AS LONG

    PUBLIC SUB NEW(marca AS STRING, cilindrata AS LONG)
        BASE.NEW(marca)
        SELF.Cilindrata = cilindrata
    END SUB

    PUBLIC OVERRIDE FUNCTION Tipo() AS STRING
        FUNCTION = "moto " + LTRIM$(STR(SELF.Cilindrata)) + "cc"
    END FUNCTION
END CLASS

SUB Main()
    PRINT "== Classi — INHERITS qualificato / cross-file (Z3) =="
    PRINT

    ' --- Instanze di classi con base importata ---
    DIM a AS Auto = NEW Auto("Fiat", 5)
    DIM m AS Moto = NEW Moto("Ducati", 1100)

    a.Accelera(80)
    m.Accelera(120)

    ' I campi della base importata si leggono tramite i metodi ereditati
    PRINT ">> Comportamento delle singole istanze"
    PRINT "   "; a.GetMarca(); " ("; a.Tipo(); ") velocita "; a.GetVelocita(); " km/h, chilometri "; a.Chilometri()
    PRINT "   "; m.GetMarca(); " ("; m.Tipo(); ") velocita "; m.GetVelocita(); " km/h"
    PRINT

    ' --- Polimorfismo: la funzione accetta la BASE importata (dispatch dinamico) ---
    ' NB: l'ARRAY del tipo base importato + accesso ai campi tramite metodi della base è
    ' FRAGILE nel PCodeVM (bug, vedi Bugs/Classi-Bug-Report.md): qui usiamo il passaggio
    ' a una FUNCTION tipizzata AS Mezzi.Veicolo, che funziona.
    PRINT ">> Polimorfismo con la base importata (parametro AS Mezzi.Veicolo)"
    PRINT "   "; Descrivi(a)
    PRINT "   "; Descrivi(m)
    PRINT

    PRINT ">> Fine programma (i DESTRUCTOR delle globali girano alla chiusura)"
END SUB

' La funzione riceve la BASE e invoca il metodo VIRTUAL: il dispatch chiama l'OVERRIDE
' dell'oggetto reale (Auto o Moto), non il metodo della base.
FUNCTION Descrivi(v AS Mezzi.Veicolo) AS STRING
    Descrivi = v.GetMarca() + " -> " + v.Tipo()
END FUNCTION
