' MiniTest: superficie canonica di QUEUE e STACK sulle 4 gambe.
' QUEUE: FIFO — Front = testa (prossimo Dequeue), Back = coda; Dequeue toglie la testa.
' STACK: LIFO — Front/Peek = CIMA (prossimo Pop), Back = fondo; ToArray/JSON/FOR EACH
' sono TOP-FIRST come .NET Stack<T>.
' Prima: PCode rendeva Front/Back vuoti su STACK; C99 aveva lo stack bottom-first
' (Front=10/Back=20, ToArray [10,20]) e VM/C rendeva null per JSON/bare di QUEUE/STACK.
' EXPECT: q.count= 2|q.peek= 10|q.front= 10|q.back= 20|q.contains=-1|q.isempty= 0|q.toarr=[10,20]|q.tolist=[10,20]|q.tojson="[10,20]"|q.bare=[10,20]|q.deq= 10|q.after=[20]|q.cleared= 0|s.count= 2|s.peek= 20|s.front= 20|s.back= 10|s.contains=-1|s.isempty= 0|s.toarr=[20,10]|s.tojson="[20,10]"|s.bare=[20,10]|s.pop= 20|s.after=[10]|s.cleared= 0
IMPORT JSON
SUB Main()
  DIM q AS QUEUE<INTEGER>
  q.Enqueue(10)
  q.Enqueue(20)
  PRINT "q.count="; q.Count()
  PRINT "q.peek="; q.Peek()
  PRINT "q.front="; q.Front()
  PRINT "q.back="; q.Back()
  PRINT "q.contains="; q.Contains(20)
  PRINT "q.isempty="; q.IsEmpty()
  PRINT "q.toarr="; JSON.STRINGIFY(q.ToArray())
  PRINT "q.tolist="; JSON.STRINGIFY(q.ToList())
  PRINT "q.tojson="; JSON.STRINGIFY(q.ToJson())
  PRINT "q.bare="; JSON.STRINGIFY(q)
  PRINT "q.deq="; q.Dequeue()
  PRINT "q.after="; JSON.STRINGIFY(q.ToArray())
  q.Clear()
  PRINT "q.cleared="; q.Count()

  DIM s AS STACK<INTEGER>
  s.Push(10)
  s.Push(20)
  PRINT "s.count="; s.Count()
  PRINT "s.peek="; s.Peek()
  PRINT "s.front="; s.Front()
  PRINT "s.back="; s.Back()
  PRINT "s.contains="; s.Contains(10)
  PRINT "s.isempty="; s.IsEmpty()
  PRINT "s.toarr="; JSON.STRINGIFY(s.ToArray())
  PRINT "s.tojson="; JSON.STRINGIFY(s.ToJson())
  PRINT "s.bare="; JSON.STRINGIFY(s)
  PRINT "s.pop="; s.Pop()
  PRINT "s.after="; JSON.STRINGIFY(s.ToArray())
  s.Clear()
  PRINT "s.cleared="; s.Count()
END SUB
