' RINGBUFFER: capacità di default (64, parità C# CircularQueue/P-Code) e Count/Capacity.
' EXPECT: cap= 64|count= 3|cap2= 64
IMPORT COLLECTIONS
SUB Main()
  DIM r AS RINGBUFFER<LONG>
  PRINT "cap="; r.Capacity()
  r.ENQUEUE(1) : r.ENQUEUE(2) : r.ENQUEUE(3)
  PRINT "count="; r.Count()
  PRINT "cap2="; r.Capacity()
END SUB
