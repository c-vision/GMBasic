' MiniTest: array slice a[start:end] (SysCall.ARRAY_SLICE = 271).
' Sul VMC la tabella instradava 271 a vm_sys_json (pop di UNA stringa + push di un dict):
' silent-wrong E stack sbilanciato. Ora -> vm_sys_array_slice (parita' PCodeVM/IL).
' Oracolo: elementi [start, min(end,count)) in una NUOVA lista.
' EXPECT: s0= 2|s1= 3|n= 2|full= 4|tail= 2
DIM a AS LIST OF INTEGER = [1, 2, 3, 4]

DIM s AS LIST OF INTEGER
s = a[1:3]
PRINT "s0="; s[0]
PRINT "s1="; s[1]
PRINT "n="; s.Count

DIM f AS LIST OF INTEGER
f = a[0:4]
PRINT "full="; f.Count

DIM t AS LIST OF INTEGER
t = a[2:9]
PRINT "tail="; t.Count
