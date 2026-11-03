' MiniTest HASHSET — set algebra: Union/Intersect/Except/SymmetricExcept.
' A={1,2,3,4} B={3,4,5,6}: |U|=6 |I|=2 |A\B|=2 |A△B|=4.
' EXPECT: u= 6|i= 2|e= 2|s= 4|u1=-1|i3=-1|e1=-1|e3= 0|s1=-1|s3= 0|s5=-1
IMPORT COLLECTIONS

SUB Main()
    DIM A AS HASHSET<INTEGER> = [1, 2, 3, 4]
    DIM B AS HASHSET<INTEGER> = [3, 4, 5, 6]
    DIM U = A.Union(B)
    DIM I = A.Intersect(B)
    DIM E = A.Except(B)
    DIM S = A.SymmetricExcept(B)

    PRINT "u="; U.Count()
    PRINT "i="; I.Count()
    PRINT "e="; E.Count()
    PRINT "s="; S.Count()
    PRINT "u1="; U.Contains(1)
    PRINT "i3="; I.Contains(3)
    PRINT "e1="; E.Contains(1)
    PRINT "e3="; E.Contains(3)
    PRINT "s1="; S.Contains(1)
    PRINT "s3="; S.Contains(3)
    PRINT "s5="; S.Contains(5)
END SUB
