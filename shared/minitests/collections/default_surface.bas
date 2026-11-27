' MiniTest: superficie dei COSTRUTTORI DI DEFAULT di tutte le collezioni (DIM senza NEW)
' sulle 4 gambe. Ogni collezione è "pronta all'uso": Count=0, JSON canonico e TYPENAME
' coerente col canone delle mappe/sequenze (le strutture avanzate riportano il nome
' canonico della loro rappresentazione: SORTEDDICTIONARY`2 per BPlusTree/BITree,
' BINARYHEAP`1 per HEAP/MAXHEAP, LIST`1 per CIRCULAR, ecc.).
' Divergenze corrette il 2026-11-26:
'  - BPLUSTREE senza NEW non compilava su IL (ctor reale (int, IComparer) — GetConstructor
'    a 1 argomento era null → ArgumentNullException('con') in ILGEN);
'  - TYPENAME: BPlusTree/BITree davano "BPLUSTREE`2"/"BINARYINDEXEDTREE``1" solo su IL
'    (ora SORTEDDICTIONARY`2 come PCode/VM-C/C99) e HEAP/MAXHEAP davano LIST`1 su
'    PCode/VM-C/C99 contro BINARYHEAP`1 di IL (ora 4 gambe = BINARYHEAP`1);
'  - JSON di BITree {"Count":N} (VM/C null, C99 []) e GRAPH {"NodeCount":N,"Nodes":[...]}
'    (VM/C null, C99 non compilava: gmb_make_val_string(<handle>.data));
'  - SPARSESET senza NEW: Capacity 64 su IL/PCode contro 0 VM/C e 8 C99.
' EXPECT: list t=LIST`1 c= 0  j=[]|dict t=DICTIONARY`2 c= 0  j={}|queue t=QUEUE`1 c= 0  j=[]|stack t=STACK`1 c= 0  j=[]|hset t=HASHSET`1 c= 0  j=[]|sorted t=SORTEDDICTIONARY`2 c= 0  j={}|tree t=SORTEDDICTIONARY`2 c= 0  j={}|avl t=SORTEDDICTIONARY`2 c= 0  j={}|rb t=SORTEDDICTIONARY`2 c= 0  j={}|bpt t=SORTEDDICTIONARY`2 c= 0  j={}|cache t=CACHE`2 c= 0  j={}|timeline t=TIMELINE`1 c= 0  j={}|trie t=TRIE c= 0  j=[]|linked t=LINKEDLIST`1 c= 0  j=[]|doubly t=LINKEDLIST`1 c= 0  j=[]|circ t=LIST`1 c= 0  j=[]|heap t=BINARYHEAP`1 c= 0  j=[]|maxheap t=BINARYHEAP`1 c= 0  j=[]|bag t=DICTIONARY`2 c= 0  j={}|bit t=SORTEDDICTIONARY`2 c= 0  j={"Count":0}|ring t=CIRCULARQUEUE`1 c= 0  cap= 64  j=[]|sparse t=SPARSESET c= 0  cap= 64  j=[]|hll t=HLL c= 0  p= 14|graph t=GRAPH`1 c= 0  j={"NodeCount":0,"Nodes":[]}
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM l AS LIST<INTEGER> : PRINT "list t="; TYPENAME(l); " c="; l.Count(); " j="; JSON.STRINGIFY(l)
  DIM d AS DICT<STRING, INTEGER> : PRINT "dict t="; TYPENAME(d); " c="; d.Count(); " j="; JSON.STRINGIFY(d)
  DIM q AS QUEUE<INTEGER> : PRINT "queue t="; TYPENAME(q); " c="; q.Count(); " j="; JSON.STRINGIFY(q)
  DIM st AS STACK<INTEGER> : PRINT "stack t="; TYPENAME(st); " c="; st.Count(); " j="; JSON.STRINGIFY(st)
  DIM hs AS HASHSET<INTEGER> : PRINT "hset t="; TYPENAME(hs); " c="; hs.Count(); " j="; JSON.STRINGIFY(hs)
  DIM so AS SORTED<STRING, INTEGER> : PRINT "sorted t="; TYPENAME(so); " c="; so.Count(); " j="; JSON.STRINGIFY(so)
  DIM tr AS TREE<STRING, INTEGER> : PRINT "tree t="; TYPENAME(tr); " c="; tr.Count(); " j="; JSON.STRINGIFY(tr)
  DIM av AS AVLTREE<STRING, INTEGER> : PRINT "avl t="; TYPENAME(av); " c="; av.Count(); " j="; JSON.STRINGIFY(av)
  DIM rb AS REDBLACK<STRING, INTEGER> : PRINT "rb t="; TYPENAME(rb); " c="; rb.Count(); " j="; JSON.STRINGIFY(rb)
  DIM bp AS BPLUSTREE<STRING, INTEGER> : PRINT "bpt t="; TYPENAME(bp); " c="; bp.Count(); " j="; JSON.STRINGIFY(bp)
  DIM ca AS CACHE<STRING, INTEGER> : PRINT "cache t="; TYPENAME(ca); " c="; ca.Count(); " j="; JSON.STRINGIFY(ca)
  DIM tl AS TIMELINE<STRING> : PRINT "timeline t="; TYPENAME(tl); " c="; tl.Count(); " j="; JSON.STRINGIFY(tl)
  DIM tri AS TRIE : PRINT "trie t="; TYPENAME(tri); " c="; tri.Count(); " j="; JSON.STRINGIFY(tri)
  DIM ll AS LINKEDLIST<INTEGER> : PRINT "linked t="; TYPENAME(ll); " c="; ll.Count(); " j="; JSON.STRINGIFY(ll)
  DIM dl AS DOUBLYLINKEDLIST<INTEGER> : PRINT "doubly t="; TYPENAME(dl); " c="; dl.Count(); " j="; JSON.STRINGIFY(dl)
  DIM ci AS CIRCULAR<INTEGER> : PRINT "circ t="; TYPENAME(ci); " c="; ci.Count(); " j="; JSON.STRINGIFY(ci)
  DIM he AS HEAP<INTEGER> : PRINT "heap t="; TYPENAME(he); " c="; he.Count(); " j="; JSON.STRINGIFY(he)
  DIM mh AS MAXHEAP<INTEGER> : PRINT "maxheap t="; TYPENAME(mh); " c="; mh.Count(); " j="; JSON.STRINGIFY(mh)
  DIM bg AS BAG<STRING> : PRINT "bag t="; TYPENAME(bg); " c="; bg.Count(); " j="; JSON.STRINGIFY(bg)
  DIM bt AS BITREE<INTEGER> : PRINT "bit t="; TYPENAME(bt); " c="; bt.Count(); " j="; JSON.STRINGIFY(bt)
  DIM rbf AS RINGBUFFER<INTEGER> : PRINT "ring t="; TYPENAME(rbf); " c="; rbf.Count(); " cap="; rbf.Capacity(); " j="; JSON.STRINGIFY(rbf)
  DIM sp AS SPARSESET : PRINT "sparse t="; TYPENAME(sp); " c="; sp.Count(); " cap="; sp.Capacity(); " j="; JSON.STRINGIFY(sp)
  DIM hl AS HLL : PRINT "hll t="; TYPENAME(hl); " c="; hl.Count(); " p="; hl.Precision()
  DIM gr AS GRAPH<STRING> : PRINT "graph t="; TYPENAME(gr); " c="; gr.NodeCount(); " j="; JSON.STRINGIFY(gr)
END SUB
