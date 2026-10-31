' ModelCompilerModules.bas — GMB MODULE: registry di moduli esterni (model compiler §4.3).
' Un modulo è una dylib nativa registrata su uno SLOT a priorità; replace/enrich per priorità.
SUB Main()
  ' Registra due dylib sullo stesso slot: vince quella a priorità più alta.
  GMB MODULE REGISTER "quant.emitter", "/usr/lib/libSystem.B.dylib"
  GMB MODULE REGISTER "quant.emitter", "/usr/lib/libc++.1.dylib"
  GMB MODULE PRIORITY "quant.emitter", 10
  ' Elenca i moduli registrati (slot, dylib, priorità).
  GMB MODULE LIST
  PRINT "registry pronto"
END SUB
