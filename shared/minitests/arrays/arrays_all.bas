' MiniTest NAMESPACE ARRAYS — tutti i 26 simboli della superficie ARRAYS.
'
' Superficie (shared/spec/registry/surface.csv): AADD, ABINARYSEARCH, ACHUNK,
' ACLONE, ACOPY, ACOUNT, ADEL, AEVAL, AFILL, AFILTER, AFLAT, AINS, AJOIN, AMAP,
' AMAX, AMIN, APAD, AREDUCE, AREVERSE, ASCAN, ASHUFFLE, ASORT, ASUM, ATOKENS,
' AUNIQUE, AZIP.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Oracolo deterministico (ASHUFFLE viene riordinato con ASORT).
' EXPECT: rev= 2 , 1 , 4 , 1 , 3|uniqN= 4 |uniq= 3 , 1 , 4 , 2|clone0= 3 |src0= 9|copy= 3 , 2|scan4= 2 |scan99=-1|join=3-1-4-1-2|sort= 1 , 1 , 2 , 3 , 4|bs2= 2 |bs9=-1|map= 2 , 2 , 4 , 6 , 8|filterN= 3 |filter= 1 , 1 , 3|reduce= 11|count= 2|max= 4 |min= 1 |sum= 11|flatN= 4 |flat= 1 , 2 , 3 , 4|padN= 7 |pad5= 0 |pad6= 0|tokN= 3 |tok=a,b,c|zipN= 3|chunkN= 3|addN= 3 |add2= 30|insN= 4 |ins1= 0|delN= 3 |del0= 0|fill= 7 , 7|shuffle= 1 , 2 , 3 , 4 , 5|desc= 5 , 1|aeval=ok
IMPORT ARRAYS

SUB Main()
  DIM a(4) = {3, 1, 4, 1, 2}

  ' ── trasformazioni che RITORNANO un nuovo array ───────────────────────────
  DIM rev = ARRAYS.AREVERSE(a)
  PRINT "rev="; rev(0); ","; rev(1); ","; rev(2); ","; rev(3); ","; rev(4)

  DIM uni = ARRAYS.AUNIQUE(a)
  PRINT "uniqN="; UBOUND(uni) + 1; "|uniq="; uni(0); ","; uni(1); ","; uni(2); ","; uni(3)

  ' ── copie e ricerca/valore ───────────────────────────────────────────────
  DIM cl = ARRAYS.ACLONE(a)
  a(0) = 9
  PRINT "clone0="; cl(0); "|src0="; a(0)
  a(0) = 3

  DIM cp(4)
  ACOPY a, cp
  PRINT "copy="; cp(0); ","; cp(4)

  PRINT "scan4="; ASCAN(a, 4); "|scan99="; ASCAN(a, 99)
  PRINT "join="; ARRAYS.AJOIN(a, "-")

  DIM srt = ARRAYS.ASORT(a)
  PRINT "sort="; srt(0); ","; srt(1); ","; srt(2); ","; srt(3); ","; srt(4)
  PRINT "bs2="; ARRAYS.ABINARYSEARCH(srt, 2); "|bs9="; ARRAYS.ABINARYSEARCH(srt, 9)

  DIM mp = ARRAYS.AMAP(a, FN(x) => x * 2)
  PRINT "map="; mp(0); ","; mp(1); ","; mp(2); ","; mp(3); ","; mp(4)

  DIM flt = ARRAYS.AFILTER(a, FN(x) => (x MOD 2) = 1)
  PRINT "filterN="; UBOUND(flt) + 1; "|filter="; flt(0); ","; flt(1); ","; flt(2)

  PRINT "reduce="; ARRAYS.AREDUCE(a, FN(acc, x) => acc + x)
  PRINT "count="; ARRAYS.ACOUNT(a, FN(x) => x > 2)
  PRINT "max="; ARRAYS.AMAX(a); "|min="; ARRAYS.AMIN(a); "|sum="; ARRAYS.ASUM(a)

  DIM nested = {{1, 2}, {3, 4}}
  DIM flat = ARRAYS.AFLAT(nested)
  PRINT "flatN="; UBOUND(flat) + 1; "|flat="; flat(0); ","; flat(1); ","; flat(2); ","; flat(3)

  DIM pad = ARRAYS.APAD(a, 7, 0)
  PRINT "padN="; UBOUND(pad) + 1; "|pad5="; pad(5); "|pad6="; pad(6)

  DIM tk = ARRAYS.ATOKENS("a,,b;c", ",;")
  PRINT "tokN="; UBOUND(tk) + 1; "|tok="; tk(0); ","; tk(1); ","; tk(2)

  DIM a1(2) = {10, 20, 30}
  DIM a2(2) = {100, 200, 300}
  DIM zp = ARRAYS.AZIP(a1, a2)
  PRINT "zipN="; UBOUND(zp) + 1
  DIM ch = ARRAYS.ACHUNK(a, 2)
  PRINT "chunkN="; UBOUND(ch) + 1

  ' ── mutazioni in place ───────────────────────────────────────────────────
  DIM d() = {10, 20}
  AADD d, 30
  PRINT "addN="; UBOUND(d) + 1; "|add2="; d(2)

  AINS d, 1
  PRINT "insN="; UBOUND(d) + 1; "|ins1="; d(1)

  ADEL d, 0
  PRINT "delN="; UBOUND(d) + 1; "|del0="; d(0)

  DIM fldata(3)
  AFILL fldata, 7
  PRINT "fill="; fldata(0); ","; fldata(3)

  DIM sh(4) = {5, 3, 1, 4, 2}
  ASHUFFLE sh
  ASORT sh
  PRINT "shuffle="; sh(0); ","; sh(1); ","; sh(2); ","; sh(3); ","; sh(4)

  ASORT sh, , "D"
  PRINT "desc="; sh(0); ","; sh(4)

  ' AEVAL: esegue la lambda su ogni elemento (effetto collaterale)
  ARRAYS.AEVAL(a, SUB(x) { IF x = 4 THEN PRINT "aeval=ok" })
END SUB
