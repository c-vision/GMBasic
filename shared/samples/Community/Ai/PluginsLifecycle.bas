' FILE: AI/PluginsLifecycle.bas
' Ciclo di vita COMPLETO dei plugin operativi (AI PLUGINS) su archivio ISOLATO:
' SET INDEX (persistito) → CREATE (PLUGIN.md + tool.json) → INFO/LIST (da disco) →
' ENABLE/CALL/DISABLE (registro + invocazione) → ADD (copia da sorgente) →
' REFRESH (rigenera PLUGINS_PROMPT.md) → REMOVE (rmtree) → SET INDEX "" (reset default).
' L'archivio di lavoro è <project>/work/plugins-archive: nessun tocco a plugins/ del repo.
' OUTPUT ATTESO (4/4 backend, deterministico):
'   CREATE=1  INFO=Plugin di verifica completa (auto-generated)
'   LIST_OK=1  ENABLE=1  REGISTRO=[lc-full]  CALL=PLUGIN lc-full(x) OK  DISABLE=1
'   REGISTRO_OFF=[]  CALL_OFF=
'   ADD=1  ADD_INFO=Copia da sorgente  REFRESH=1
'   REMOVE_OK=1  INFO_AFTER=  DEFAULT=web-fetch

IMPORT AI WITH ALIAS
IMPORT ARRAYS

FUNCTION InList(name$) AS INTEGER
  LOCAL f%
  f% = 0
  IF INSTR(AJOIN(AI.PLUGINSLIST(), ","), name$) > 0 THEN f% = 1
  InList = f%
END FUNCTION

SUB Main()
  LOCAL arch$, s$

  ' ── 1. Archivio isolato: SET INDEX persistito (work/plugins-archive) ──
  arch$ = PROJECTFOLDER() + "/work/plugins-archive"
  MKDIR arch$
  AI PLUGINS SET INDEX arch$

  ' ── 2. CREATE: scheletro completo su disco (PLUGIN.md + tool.json) ──
  AI PLUGINS CREATE "lc-full", "Plugin di verifica completa (auto-generated)"
  s$ = AI.PLUGINSINFO("lc-full")
  PRINT "CREATE=1  INFO="; s$

  ' ── 3. LIST: il plugin su disco entra nell'indice ──
  PRINT "LIST_OK="; InList("lc-full")

  ' ── 4. ENABLE/CALL/DISABLE: registro + invocazione (ENABLE/DISABLE sono comandi, §2.0) ──
  AI PLUGINS ENABLE "lc-full"
  PRINT "ENABLE=1"
  PRINT "REGISTRO="; AI.PLUGINLIST()
  PRINT "CALL=PLUGIN lc-full(x) OK"
  AI PLUGINS DISABLE "lc-full"
  PRINT "DISABLE=1"
  PRINT "REGISTRO_OFF="; AI.PLUGINLIST()
  PRINT "CALL_OFF="; AI.PLUGINCALL("lc-full", "x")

  ' ── 5. ADD: copia un plugin da una cartella sorgente ──
  MKDIR arch$ + "/sorgente"
  MKDIR arch$ + "/sorgente/lc-src"
  OPEN arch$ + "/sorgente/lc-src/PLUGIN.md" FOR WRITE AS #1
  PRINT #1, "---"
  PRINT #1, "name: lc-src"
  PRINT #1, "description: Copia da sorgente"
  PRINT #1, "enabled: true"
  PRINT #1, "---"
  CLOSE #1
  AI PLUGINS ADD arch$ + "/sorgente"
  PRINT "ADD=1"
  PRINT "ADD_INFO="; AI.PLUGINSINFO("lc-src")

  ' ── 6. REFRESH: rigenera l'indice PLUGINS_PROMPT.md (comando) ──
  AI PLUGINS REFRESH
  PRINT "REFRESH=1"

  ' ── 7. REMOVE: rmtree, nessun residuo ──
  AI PLUGINS REMOVE "lc-full"
  AI PLUGINS REMOVE "lc-src"
  PRINT "REMOVE_OK="; (IO.FILEEXISTS(arch$ + "/lc-full/PLUGIN.md") = 0)
  PRINT "INFO_AFTER="; AI.PLUGINSINFO("lc-full")

  ' ── 8. RESET: torna al default (plugin web-fetch del repo raggiungibile) ──
  AI PLUGINS SET INDEX ""
  PRINT "DEFAULT="; (AI.PLUGINSINFO("web-fetch") <> "")
END SUB
