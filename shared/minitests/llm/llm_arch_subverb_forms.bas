' EXPECT: LLM ARCH 'tiny_arch' ()|ruoli (0):|quant (0):|recipe= boost_fmt= scale_suffix=|modality=text source.format=safetensors mmproj=|LLM ARCH 'tiny_arch' ()|ruoli (0):|quant (0):|recipe= boost_fmt= scale_suffix=|modality=text source.format=safetensors mmproj=
' MiniTest: `LLM ARCH` — il SOTTO-VERBO vale sia come PAROLA NUDA sia come STRINGA:
'   LLM ARCH SHOW        (forma canonica, quella dei sample e delle guide)
'   LLM ARCH "SHOW"      (forma richiesta dal compilatore chunk: `go_args[0]` è un'espressione)
' Le due forme devono dare lo STESSO risultato su TUTTE le gambe (IL, PCodeVM, VM/C, C99Native).
'
' BUG COPERTO (2026-11-11): sul VM/C il primo argomento passava da `cc_expr`, che per un VAR nudo
' emette il LOAD di una variabile inesistente → stringa vuota → "LLM ARCH: sotto-verbo '' non
' supportato"; sul C99Native la parola nuda funzionava ma la forma fra virgolette no. Ora il
' compilatore chunk usa `cc_push_str_arg` (come l'emitter C99 legge `a[0]->var`) e l'emitter C99
' accetta anche `a[0]->str`.
'
' Fixture `tiny_arch`: modello SENZA descrittore dedicato → RIPIEGO GENERICO deterministico
' (arch dal config.json, nessun ruolo/quant, path vuoto). Nessun modello reale, nessuna ABI esterna.
IMPORT LLM
SUB Main()
  LLM ARCH RESOLVE "../../samples/Fixtures/models/tiny_arch"
  LLM ARCH SHOW
  LLM ARCH "SHOW"
END SUB
