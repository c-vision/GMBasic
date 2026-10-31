' MiniTest: CURSOR nudo (gruppo Console, parità 4 backend).
' CURSOR SHAPE mancava sull'emitter C#; CURSOR intero assente sul C99Native
' (2026-09-21: caso CursorShapeStmt + GMB_STMT_CURSOR con parse/emit).
' Il cursore non produce output (ANSI solo su tty): l'oracolo è il marker finale,
' che fallisce se una gamba inquina lo stdout.
' EXPECT: cursor=ok
IMPORT CONSOLE

SUB Main()
  CURSOR OFF
  CURSOR SHAPE "BAR"
  CURSOR SHAPE "UNDERLINE"
  CURSOR SHAPE "BLOCK"
  CURSOR SHAPE 2
  CURSOR ON
  PRINT "cursor=ok"
END SUB
