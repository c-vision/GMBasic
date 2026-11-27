' MiniTest: operazioni su QUEUE/STACK/HEAP/SPARSESET VUOTE — canone NIL su 4 gambe.
' Dequeue()/Pop()/Peek() su coda/pila vuota rendono NIL (stampato vuoto), MAI
' l'eccezione CLR "Queue empty"/"Stack empty" (prima: IL lanciava
' InvalidOperationException, PCode "[P-CODE ERROR] Queue empty").
' Peek()/Extract() di un HEAP vuoto = null in JSON; HasItem/Remove di SPARSESET
' vuoto = 0. La forma vuota di JSON.STRINGIFY(heap.Peek()) non compilava su C99
' (gmb_val_to_json su un double): ora serializza il gmb_val_t.
' EXPECT: q=// 0|s=// 0|h=null/null|sp= 0 / 0|rb=// 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM q AS QUEUE<INTEGER>
  PRINT "q="; q.Dequeue(); "/"; q.Peek(); "/"; q.Count()
  DIM st AS STACK<INTEGER>
  PRINT "s="; st.Pop(); "/"; st.Peek(); "/"; st.Count()
  DIM he AS HEAP<INTEGER>
  PRINT "h="; JSON.STRINGIFY(he.Peek()); "/"; JSON.STRINGIFY(he.Extract())
  DIM sp AS SPARSESET
  PRINT "sp="; sp.HasItem(3); "/"; sp.Remove(3)
  DIM rb AS RINGBUFFER<INTEGER>
  PRINT "rb="; rb.Dequeue(); "/"; rb.Peek(); "/"; rb.Count()
END SUB
