' EXPECT: not2=-1
' Parita' IL/PCodeVM/VM-C per NOT (n IS NIL). NOTA: questo minitest, eseguito via
' tools/test_minitests_parity, NON esercita ILOptim (RunIl in MiniTestsParityTests.cs chiama
' CompileToAssembly direttamente, senza il passo ILOptimOptimizer.Optimize della pipeline reale
' IlProgramCompiler.Compile) — quindi da solo non copre il bug "IL-NOT-Bool-ILOptim-Miscompila"
' (NOT su un operando bool/narrow dava 255 invece di -1 SOLO con ILOptim attivo). L'oracolo che
' esercita davvero ILOptim e' tests/GMBASIC.IlBackend.Tests/ILOptimRegressionTests.cs (compila con
' IlProgramCompiler.Compile ed esegue come sottoprocesso). Causa/fix: ConstFolding.FoldUnary
' (common/ILOptim/IR/Utils/ConstFolding.cs) troncava ~0/-0 al tipo STRETTO dell'operando (es. Bool,
' non segnato) invece di normalizzare alla StackType (Int32/Int64) come fa davvero lo stack CIL.
SUB Main()
  DIM n AS INTEGER?
  n = 42
  PRINT "not2="; NOT (n IS NIL)
END SUB
