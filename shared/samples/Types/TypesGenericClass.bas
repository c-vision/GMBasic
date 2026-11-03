' FILE: Types/TypesGenericClass.bas
' Generic CLASS: CLASS Name<T1, T2> ... END CLASS
' Stesso schema di TypesGenericType.bas (TYPE generico), ma per CLASS: verifica che
' un campo, un costruttore e un metodo che referenziano T siano monomorfizzati
' correttamente dal Linker (ResolveGenericClassInstantiation) per un'istanziazione
' concreta (Box<DOUBLE>), end-to-end su una CLASS reference-type reale (non un UDT
' value-type). Scope MVP: solo campi/costruttore/metodi — niente ereditarietà,
' interfacce, PROPERTY, OPERATOR o DELETE insieme ai generici (non ancora
' supportato, lancia esplicito).

IMPORT SYSTEM

CLS
PRINT "=== Generic CLASS<T> ==="
PRINT

CLASS Box<T>
    PUBLIC DIM Value AS T

    PUBLIC SUB NEW(v AS T)
        SELF.Value = v
    END SUB

    PUBLIC FUNCTION GetValue() AS T
        GETVALUE = SELF.Value
    END FUNCTION
END CLASS

PRINT "=== Istanziazione generica: Box<DOUBLE> ==="
DIM b AS Box<DOUBLE>
b = NEW Box<DOUBLE>(42.5)
PRINT "b.GetValue() = "; b.GetValue()
' (nessun oracolo: è un Sample, non un MiniTest)

PRINT
PRINT "=== Fine ==="
END
