' FILE: Arrays/ArraysFluent.bas
' Array fluent methods: arr.Filter(...).Map(...)
' Syntax: arr.Method(args) ~ ARRAYS.AMethod(arr, args)

SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════╗"
    PRINT "║ ArraysFluent - Array fluent methods                          ║"
    PRINT "╚══════════════════════════════════════════════════════════════╝"
    PRINT

    DIM arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}

    ' arr.Filter(fn) ~ ARRAYS.AFILTER(arr, fn)
    DIM even = ARRAYS.AFILTER(arr, FN(x) => x MOD 2 = 0)
    PRINT "Pari: "; ARRAYS.AJOIN(even, ",")

    ' arr.Map(fn) ~ ARRAYS.AMAP(arr, fn)
    DIM doubled = ARRAYS.AMAP(arr, FN(x) => x * 2)
    PRINT "Raddoppiati: "; ARRAYS.AJOIN(doubled, ",")

    ' arr.Reduce(fn, init) ~ ARRAYS.AREDUCE(arr, fn, init)
    DIM sum = ARRAYS.AREDUCE(arr, FN(acc, x) => acc + x, 0)
    PRINT "Somma: "; sum

    ' arr.Sort() ~ ARRAYS.ASORT(arr)
    DIM unsorted = {3, 1, 4, 1, 5, 9, 2, 6}
    DIM sorted = ARRAYS.ASORT(unsorted)
    PRINT "Ordinato: "; ARRAYS.AJOIN(sorted, ",")

    ' arr.Flat() ~ ARRAYS.AFLAT(arr)
    DIM nested = {{1, 2}, {3, 4}, {5, 6}}
    DIM flat = ARRAYS.AFLAT(nested)
    PRINT "Appiattito: "; ARRAYS.AJOIN(flat, ",")

    ' arr.Reverse() ~ ARRAYS.AREVERSE(arr)
    DIM rev = ARRAYS.AREVERSE(arr)
    PRINT "Invertito: "; ARRAYS.AJOIN(rev, ",")

    ' arr.Unique() ~ ARRAYS.AUNIQUE(arr)
    DIM withDupes = {1, 2, 2, 3, 3, 3, 4, 5}
    DIM uniq = ARRAYS.AUNIQUE(withDupes)
    PRINT "Unici: "; ARRAYS.AJOIN(uniq, ",")

    ' arr.Count(fn) ~ ARRAYS.ACOUNT(arr, fn)
    DIM cnt = ARRAYS.ACOUNT(arr, FN(x) => x > 5)
    PRINT "Elementi > 5: "; cnt

    ' Chaining: arr.Filter(fn1).Map(fn2) ~ usando variabile intermedia
    DIM filtered = ARRAYS.AFILTER(arr, FN(x) => x MOD 2 <> 0)
    DIM result = ARRAYS.AMAP(filtered, FN(x) => x * 10)
    PRINT "Dispari*10: "; ARRAYS.AJOIN(result, ",")

    PRINT "OK. Array fluent test completato."
END SUB
