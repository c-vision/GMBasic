' PromptBudgetReport.bas — report completo della superficie LLM "come valore".
'
' Usa OGNI funzione/comando introdotto il 2026-10-30 e le rispettive forme con INTO:
'   LLM.TOKENS        testualizza il prompt e ne ricava gli id
'   LLM.TOKENCOUNT    verifica il budget di token
'   LLM.DETOKENIZE    round-trip: gli id devono tornare al testo originale
'   LLM.TENSOR        info del tokenizer (sotto-verbo TOKENIZER) come valore
'   LLM.POLICY        tabella ruolo→formato; SET/RESET di una regola
'   LLM.ARCH          descrizione d'architettura (SHOW)
'   LLM.MODULE/KERNEL/KV  interrogati con guardia (richiedono un modello reale: ON ERROR)
'   LLM … INTO        stesse chiamate in forma-comando, senza stampa
'
' Uso: PromptBudgetReport.bas [tokenizer]
' Exit: 0 se il prompt rientra nel budget e il round-trip è esatto, 1 se il budget è sfondato.
IMPORT LLM

DIM g_polOk AS LONG
DIM g_modOk AS LONG
DIM g_kerOk AS LONG
DIM g_kvOk AS LONG


' Prove "con guardia": le famiglie che richiedono un modello reale possono fallire; l'errore
' deve essere CATTURABILE e non fermare il report. (ON ERROR dentro una SUB: il label con
' RETURN in una FUNCTION è ancora mal supportato dal backend IL — vedi Bugs/.)
SUB ProvaPolicy()
  ON ERROR GOTO PolFine
  DIM tmp AS STRING
  tmp = LLM.POLICY("SET", "FFN", "bf16")
  tmp = LLM.POLICY("RESET")
  g_polOk = 1
PolFine:
END SUB

SUB ProvaFamiglie()
  ' funzione e forma-comando INTO per ognuna delle tre famiglie (entrambe con guardia)
  DIM f AS STRING
  ON ERROR GOTO ModErr
  f = LLM.MODULE("SHOW")
  g_modOk = 1
  LLM MODULE SHOW INTO f
ModErr:
  ON ERROR GOTO KerErr
  f = LLM.KERNEL("CODEC")
  g_kerOk = 1
  LLM KERNEL CODEC INTO f
KerErr:
  ON ERROR GOTO KvErr
  f = LLM.KV("SHOW")
  g_kvOk = 1
  LLM KV SHOW INTO f
KvErr:
END SUB

FUNCTION Main() AS INTEGER
  DIM MAX_PROMPT AS LONG
  MAX_PROMPT = 200         ' budget di esempio (token)
  DIM tok$ AS STRING
  tok$ = "../../Fixtures/ai/tiny_tokenizer.json"
  IF COMMANDCOUNT() >= 1 THEN tok$ = COMMAND$(1)

  ' prompt realistico: istruzioni di sistema + richiesta utente
  DIM sys$ AS STRING
  DIM usr$ AS STRING
  DIM prompt$ AS STRING
  sys$ = "Sei un assistente tecnico. Rispondi in italiano, citando i file toccati."
  usr$ = "Elenca i passi per tokenizzare un prompt e verificarne il budget."
  prompt$ = sys$ + CHR$(10) + usr$

  PRINT "== PromptBudgetReport =="
  PRINT "tokenizer: " + tok$

  ' 1) TOKENS (funzione): id come dato; poi TOKENCOUNT per il budget
  DIM ids$ AS STRING
  ids$ = LLM.TOKENS(tok$, prompt$)
  DIM n AS LONG
  n = LLM.TOKENCOUNT(tok$, prompt$)
  PRINT "ids: " + LEFT$(ids$, 48) + "..."
  PRINT "token: " + LTRIM$(STR$(n)) + " / budget " + LTRIM$(STR$(MAX_PROMPT))

  ' 2) DETOKENIZE (funzione): round-trip esatto
  DIM back$ AS STRING
  back$ = LLM.DETOKENIZE(tok$, ids$)
  DIM okRound AS LONG
  okRound = IIF(back$ = prompt$, 1, 0)
  PRINT "round-trip: " + IIF(okRound = 1, "esatto", "DIVERSO")

  ' 3) TENSOR (funzione generica): info del tokenizer come valore
  DIM info$ AS STRING
  info$ = LLM.TENSOR("TOKENIZER", tok$)
  PRINT "tokenizer-info: " + IIF(INSTR(info$, "vocab_size") > 0, "disponibile", "assente")

  ' 4) POLICY: tabella come valore, poi regola SET + RESET
  DIM pol$ AS STRING
  pol$ = LLM.POLICY("SHOW")
  PRINT "policy: " + IIF(INSTR(pol$, "LLM POLICY") > 0, "tabella letta", "non disponibile")

  g_polOk = 0
  ProvaPolicy()
  PRINT "policy-set/reset: " + IIF(g_polOk = 1, "ok", "errore")

  ' 5) ARCH: descrizione come valore
  DIM arch$ AS STRING
  arch$ = LLM.ARCH("SHOW")
  PRINT "arch: " + IIF(INSTR(arch$, "LLM ARCH") > 0, "risposta ricevuta", "non disponibile")

  ' 6) MODULE/KERNEL/KV: richiedono un modello reale → guardia esplicita
  g_modOk = 0 : g_kerOk = 0 : g_kvOk = 0
  ProvaFamiglie()
  PRINT "famiglie: module=" + IIF(g_modOk = 1, "ok", "n/d") + _
        " kernel=" + IIF(g_kerOk = 1, "ok", "n/d") + _
        " kv=" + IIF(g_kvOk = 1, "ok", "n/d")

  ' 7) forme-comando con INTO: gli stessi valori, senza stampa
  DIM iTok AS STRING, iTens AS STRING, iPol AS STRING, iArch AS STRING
  LLM TOKENS tok$, prompt$ INTO iTok
  LLM TENSOR TOKENIZER tok$ INTO iTens
  LLM POLICY SHOW INTO iPol
  LLM ARCH SHOW INTO iArch
  PRINT "into: token=" + IIF(iTok = ids$, "uguale", "diverso") + _
        " tensor=" + IIF(iTens = info$, "uguale", "diverso") + _
        " policy=" + IIF(iPol = pol$, "uguale", "diverso") + _
        " arch=" + IIF(iArch = arch$, "uguale", "diverso")

  ' 8) verdetto di budget
  IF n > MAX_PROMPT THEN
    PRINT "ESITO: prompt OLTRE il budget (" + LTRIM$(STR$(n - MAX_PROMPT)) + " token in eccesso)"
    RETURN 1
  END IF
  IF okRound <> 1 THEN
    PRINT "ESITO: round-trip non esatto — superficie tokenizer incoerente"
    RETURN 2
  END IF
  PRINT "ESITO: prompt entro il budget, round-trip esatto, superficie LLM coerente"
  RETURN 0
END FUNCTION
