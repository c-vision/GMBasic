' MiniTest: F1 AgentBatch-GPM-FollowOn — AI.BLACKBOARDWAIT dentro una goroutine (GO SUB)
' deve PARCHEGGIARE il task GPM e riprendere (Strada C: l'attesa gira sul blocking pool
' via BlackboardWaitAsync, mai blocca il worker). Deterministico: il Worker parka sulla
' chiave "k" (timeout 200ms), il Main attende il WaitGroup — ordine fisso worker/got/done.
' EXPECT: worker|got=|done
IMPORT AI WITH ALIAS

SUB Worker(wg AS WAITGROUP)
  PRINT "worker"
  DIM r AS STRING
  r = AI.BLACKBOARDWAIT("bbwait-mini-key", 200)
  PRINT "got="; r
  DONE wg
END SUB

SUB Main
  CHDIR PROJECTFOLDER()
  DIM wg AS WAITGROUP
  GO SUB Worker(wg) IN wg
  WAIT wg
  PRINT "done"
END SUB
