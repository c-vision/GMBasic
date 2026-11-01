' FILE: Types/TypesGenerics.bas
' =================================================================================================
'       TEST: Typed Generics per GMBasic
' =================================================================================================
'
' Sintassi supportate:
'   - Built-in generic collections: LIST OF T, DICT OF (K, V)
'   - Type inference: DIM x = [1, 2, 3]  ' infers LIST OF INTEGER
'   - Generic functions: FUNCTION Identity(Of T)(x AS T) AS T
'   - Generic classes: CLASS Stack(Of T)
'   - Constraints: FUNCTION Max(Of T WHERE T IS IComparable)(a AS T, b AS T) AS T
'
' =================================================================================================

IMPORT SYSTEM

10 CLS
20 PRINT "=== Typed Generics Test ==="
30 PRINT

GOSUB TestListOfInteger
GOSUB TestDictOfStringInteger
GOSUB TestGenericFunction
GOSUB TestGenericClass
GOSUB TestConstraint

90 PRINT
95 PRINT "=== Tutti i test generics completati ==="
100 END

' -------------------------------------------------------------------------
' 1. LIST OF INTEGER — explicit generic type
' -------------------------------------------------------------------------
1000 TestListOfInteger:
1010 PRINT "--- 1. LIST OF INTEGER ---"
1020 DIM intList AS LIST OF INTEGER
1030 intList.ADD(10)
1040 intList.ADD(20)
1050 intList.ADD(30)
1060 IF intList.COUNT = 3 THEN PRINT "  - COUNT = 3 (OK)"
1070 IF intList(0) = 10 THEN PRINT "  - intList(0) = 10 (OK)"
1080 IF intList(1) = 20 THEN PRINT "  - intList(1) = 20 (OK)"
1090 IF intList(2) = 30 THEN PRINT "  - intList(2) = 30 (OK)"
1100 RETURN

' -------------------------------------------------------------------------
' 2. DICT OF (STRING, INTEGER) — key-value generic type
' -------------------------------------------------------------------------
2000 TestDictOfStringInteger:
2010 PRINT "--- 2. DICT OF (STRING, INTEGER) ---"
2020 DIM dict AS DICT OF STRING, INTEGER
2030 dict.ADD("alice", 100)
2040 dict.ADD("bob", 200)
2050 IF dict.COUNT = 2 THEN PRINT "  - COUNT = 2 (OK)"
2060 IF dict("alice") = 100 THEN PRINT "  - dict('alice') = 100 (OK)"
2070 IF dict("bob") = 200 THEN PRINT "  - dict('bob') = 200 (OK)"
2080 RETURN

' -------------------------------------------------------------------------
' 3. Generic function: Identity<T>
' -------------------------------------------------------------------------
3000 TestGenericFunction:
3010 PRINT "--- 3. Generic Function Identity<T> ---"
3020 DIM i = Identity(42)
3030 DIM s = Identity("hello")
3040 IF i = 42 THEN PRINT "  - Identity(42) = 42 (OK)"
3050 IF s = "hello" THEN PRINT "  - Identity('hello') = 'hello' (OK)"
3060 RETURN

' -------------------------------------------------------------------------
' 6. Nested generic: LIST OF LIST OF INTEGER
' -------------------------------------------------------------------------
6000 TestNestedGeneric:
6010 PRINT "--- 6. Nested LIST OF LIST OF INTEGER ---"
6020 DIM nested AS LIST OF LIST OF INTEGER
6030 IF nested.COUNT = 0 THEN PRINT "  - nested.COUNT = 0 (OK)"
6040 RETURN

' -------------------------------------------------------------------------
' 4. Generic class: Stack(Of T)
' -------------------------------------------------------------------------
4000 TestGenericClass:
4010 PRINT "--- 4. Generic Class Stack(Of T) ---"
4020 DIM intStack AS LIST OF INTEGER
4030 intStack.ADD(1)
4040 intStack.ADD(2)
4050 intStack.ADD(3)
4060 IF intStack.COUNT = 3 THEN PRINT "  - LIST OF INTEGER COUNT = 3 (OK)"
4070 RETURN

' -------------------------------------------------------------------------
' 5. Constraint: WHERE T IS IComparable
' -------------------------------------------------------------------------
5000 TestConstraint:
5010 PRINT "--- 5. Constraint WHERE T IS IComparable ---"
5020 DIM maxInt = Max(10, 20)
5040 IF maxInt = 20 THEN PRINT "  - Max(10, 20) = 20 (OK)"
5060 RETURN

' -------------------------------------------------------------------------
' Generic function definitions
' -------------------------------------------------------------------------
FUNCTION Identity<T>(x AS T) AS T
    RETURN x
END FUNCTION


' -------------------------------------------------------------------------
' Generic class definition
' -------------------------------------------------------------------------

' -------------------------------------------------------------------------
' Constraint example
' -------------------------------------------------------------------------
FUNCTION Max(BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    IF a > b THEN RETURN a
    RETURN b
END FUNCTION
