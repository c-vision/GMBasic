' Sample completo — operazioni insiemistiche su HASHSET
' Union / Intersect / Except / SymmetricExcept: parametri, risultati e casi limite.
' Eseguibile su IL/PCode/VM-C/C99Native con: tools/sh/parity4 <file>
IMPORT COLLECTIONS

SUB Main()
    DIM A AS HASHSET<INTEGER> = [1, 2, 3, 4]
    DIM B AS HASHSET<INTEGER> = [3, 4, 5, 6]

    DIM U = A.Union(B)
    DIM I = A.Intersect(B)
    DIM E = A.Except(B)
    DIM S = A.SymmetricExcept(B)

    PRINT "=== HASHSET set algebra ==="
    PRINT "A.Count="; A.Count(); " B.Count="; B.Count()

    ' Union: tutti gli elementi, senza duplicati.
    PRINT "Union.Count="; U.Count()
    PRINT "Union has 1/3/5="; U.Contains(1); "/"; U.Contains(3); "/"; U.Contains(5)

    ' Intersect: solo gli elementi comuni.
    PRINT "Intersect.Count="; I.Count()
    PRINT "Intersect has 3/5="; I.Contains(3); "/"; I.Contains(5)

    ' Except: elementi di A non presenti in B.
    PRINT "Except.Count="; E.Count()
    PRINT "Except has 1/3="; E.Contains(1); "/"; E.Contains(3)

    ' SymmetricExcept: elementi in A o in B, non in entrambi.
    PRINT "SymmetricExcept.Count="; S.Count()
    PRINT "Symmetric has 1/3/5="; S.Contains(1); "/"; S.Contains(3); "/"; S.Contains(5)

    ' Casi limite: differenza simmetrica con sé stesso e con un insieme disgiunto.
    DIM SelfSym = A.SymmetricExcept(A)
    PRINT "SelfSym.Count="; SelfSym.Count()

    DIM C AS HASHSET<INTEGER> = [10, 20]
    DIM Disjoint = A.SymmetricExcept(C)
    PRINT "Disjoint.Count="; Disjoint.Count()
    PRINT "Disjoint has 1/10="; Disjoint.Contains(1); "/"; Disjoint.Contains(10)
END SUB
