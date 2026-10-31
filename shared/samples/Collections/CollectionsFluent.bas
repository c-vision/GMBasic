' FILE: Collections/CollectionsFluent.bas
' Collection fluent methods: list.Filter(...).Map(...)

SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════╗"
    PRINT "║ CollectionsFluent - Collection fluent methods                 ║"
    PRINT "╚══════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- LIST ---
    DIM list AS LIST OF INTEGER
    list.Add(1)
    list.Add(2)
    list.Add(3)
    list.Add(4)
    list.Add(5)
    PRINT "List originale:"
    PRINT "  Count = "; list.Count
    PRINT "  ToArray = "; ARRAYS.AJOIN(list.ToArray(), ",")

    ' list.Sort(), list.Reverse() - already exist
    list.Reverse()
    PRINT "  Dopo Reverse: "; ARRAYS.AJOIN(list.ToArray(), ",")
    list.Sort()
    PRINT "  Dopo Sort: "; ARRAYS.AJOIN(list.ToArray(), ",")

    ' --- DICT ---
    DIM dict AS DICT OF STRING, INTEGER
    dict.Add("uno", 1)
    dict.Add("due", 2)
    dict.Add("tre", 3)
    PRINT "Dict keys: "; dict.Keys
    PRINT "Dict Count: "; dict.Count
    IF dict.ContainsKey("due") THEN PRINT "  Contiene 'due'"

    ' --- HASHSET ---
    DIM hs AS HASHSET OF INTEGER
    hs.Add(10)
    hs.Add(20)
    hs.Add(30)
    hs.Add(20)
    PRINT "HashSet ToArray: "; ARRAYS.AJOIN(hs.ToArray(), ",")
    PRINT "HashSet Count: "; hs.Count
    IF hs.Contains(20) THEN PRINT "  Contiene 20"

    ' --- QUEUE ---
    DIM q AS QUEUE OF STRING
    q.Enqueue("a")
    q.Enqueue("b")
    q.Enqueue("c")
    PRINT "Queue Peek: "; q.Peek
    PRINT "Queue Dequeue: "; q.Dequeue
    PRINT "Queue Count dopo dequeue: "; q.Count

    ' --- STACK ---
    DIM s AS STACK OF INTEGER
    s.Push(100)
    s.Push(200)
    s.Push(300)
    PRINT "Stack Pop: "; s.Pop
    PRINT "Stack Peek: "; s.Peek

    PRINT "OK. Collections fluent test completato."
END SUB
