' MiniTest — §4.1: TUTTA la superficie LLM "come valore" (funzioni + comandi con INTO).
' Copre: LLM.TOKENS, LLM.DETOKENIZE, LLM.TOKENCOUNT, LLM.TENSOR, LLM.POLICY, LLM.ARCH,
' LLM.MODULE, LLM.KERNEL, LLM.KV; le forme `LLM … INTO`; l'arità (parametri opzionali);
' il contratto d'errore (verbo inesistente → errore CATTURABILE, mai stringa vuota).
' Oracolo deterministico: fixture tokenizer del corpus + tabelle/descrizioni del compilatore.
' | EXPECT: tok=SI|detok=SI|cnt=29|tens=SI|pol=SI|polset=OK|into_pol=SI|arch=SI|archlist=SI|mod=ERR|ker=ERR|kv=ERR|into_tok=SI|into_tens=SI|into_arch=SI|ar2=SI|ar3=SI
IMPORT LLM

DIM g_out AS STRING

' Esegue una chiamata che PUO' fallire e registra "OK"/"ERR" (contratto: errore catturabile)
SUB Prova(lbl AS STRING, fam AS LONG)
  ON ERROR GOTO FamErr
  IF fam = 3 THEN g_out = LLM.MODULE("NOPE")
  IF fam = 4 THEN g_out = LLM.KERNEL("NOPE")
  IF fam = 5 THEN g_out = LLM.KV("NOPE")
  PRINT lbl + "=OK"
  GOTO Fine
FamErr:
  PRINT lbl + "=ERR"
Fine:
END SUB

SUB Main()
  DIM m$ AS STRING
  m$ = "../../samples/Fixtures/ai/tiny_tokenizer.json"

  ' ── 1) tokenizer: funzioni ────────────────────────────────────────────────
  DIM ids$ AS STRING
  ids$ = LLM.TOKENS(m$, "hello world tokenizer surface")
  PRINT IIF(LEN(ids$) > 0 AND INSTR(ids$, ",") > 0, "tok=SI", "tok=NO")

  DIM txt$ AS STRING
  txt$ = LLM.DETOKENIZE(m$, "74,71,78,78,81")
  PRINT IIF(txt$ = "hello", "detok=SI", "detok=NO")

  PRINT "cnt=" + LTRIM$(STR$(LLM.TOKENCOUNT(m$, "hello world tokenizer surface")))

  DIM info$ AS STRING
  info$ = LLM.TENSOR("TOKENIZER", m$)
  PRINT IIF(INSTR(info$, "vocab_size") > 0, "tens=SI", "tens=NO")

  ' ── 2) POLICY: SHOW (tabella) e SET/RESET (scrittura, nessun errore) ──────
  DIM pol$ AS STRING
  pol$ = LLM.POLICY("SHOW")
  PRINT IIF(INSTR(pol$, "LLM POLICY") > 0, "pol=SI", "pol=NO")

  DIM setres AS LONG
  setres = 0
  ON ERROR GOTO PolErr
  DIM unused AS STRING
  unused = LLM.POLICY("SET", "FFN", "bf16")
  unused = LLM.POLICY("RESET")
  setres = 1
PolErr:
  PRINT IIF(setres = 1, "polset=OK", "polset=ERR")

  ' confronto funzione vs INTO nello STESSO stato (dopo eventuali SET di altri test)
  DIM pol2 AS STRING
  pol2 = LLM.POLICY("SHOW")
  DIM i3 AS STRING
  LLM POLICY SHOW INTO i3
  PRINT IIF(i3 = pol2, "into_pol=SI", "into_pol=NO")

  ' ── 3) ARCH: SHOW e LIST ──────────────────────────────────────────────────
  DIM arch$ AS STRING
  arch$ = LLM.ARCH("SHOW")
  PRINT IIF(INSTR(arch$, "LLM ARCH") > 0, "arch=SI", "arch=NO")

  DIM archl$ AS STRING
  archl$ = LLM.ARCH("LIST")
  PRINT IIF(INSTR(archl$, "LLM ARCH") > 0, "archlist=SI", "archlist=NO")

  ' ── 4) MODULE/KERNEL/KV: verbo INESISTENTE → errore catturabile (portabile) ─
  Prova("mod", 3)
  Prova("ker", 4)
  Prova("kv", 5)

  ' ── 5) comandi con INTO (stesso valore delle funzioni, nessuna stampa) ────
  DIM i1 AS STRING
  LLM TOKENS m$, "hello world tokenizer surface" INTO i1
  PRINT IIF(i1 = ids$, "into_tok=SI", "into_tok=NO")

  DIM i2 AS STRING
  LLM TENSOR TOKENIZER m$ INTO i2
  PRINT IIF(i2 = info$, "into_tens=SI", "into_tens=NO")


  DIM i4 AS STRING
  LLM ARCH SHOW INTO i4
  PRINT IIF(i4 = arch$, "into_arch=SI", "into_arch=NO")

  ' ── 6) arità: 2 argomenti (TENSOR) e 3 argomenti (POLICY SET) ────────────
  DIM a2 AS STRING
  a2 = LLM.TENSOR("TOKENIZER", m$)
  PRINT IIF(INSTR(a2, "vocab_size") > 0, "ar2=SI", "ar2=NO")

  DIM a3 AS STRING
  a3 = LLM.POLICY("SET", "FFN", "bf16")
  PRINT "ar3=SI"
END SUB
