' PARITY-EXCLUDE: browser/CDP: provider per-agent ignorato sul C
' BROWSER PROVIDER PER-AGENT — sample per CUA Universal Action Runtime (Piani/CUA-Universal-Action-Runtime-Plan.md)
' Configura il browser per la sessione agente: "chromium" (default, 300MB) o "obscura" (70MB, agente)
' Richiede: gmb setup BROWSER e gmb setup OBSCURA (se si vuole obscura)

IMPORT AI WITH ALIAS

SUB Main()
  ' 1. Crea due agenti con provider diversi
  AI AGENT CREATE "researcher_chrome"
  AI AGENT CREATE "researcher_obscura"

  ' 2. Configura il browser per sessione agente (da BASIC, per-sessione)
  '    Sintassi: AI AGENT SET "nome" BROWSER %OBSCURA|%CHROMIUM|%AUTO (senza virgole)
  PRINT "Imposto browser per agente..."
  AI AGENT SET "researcher_chrome" BROWSER %CHROMIUM
  PRINT "researcher_chrome -> chromium OK"
  AI AGENT SET "researcher_obscura" BROWSER %OBSCURA
  PRINT "researcher_obscura -> obscura OK"

  ' 3. Verifica (funzione)
  PRINT "researcher_chrome browser="; AI.AGENTGET("researcher_chrome", "browser")
  PRINT "researcher_obscura browser="; AI.AGENTGET("researcher_obscura", "browser")

  ' 4. Stato completo (include browser)
  PRINT AI.AGENTSTATUS("researcher_chrome")
  PRINT AI.AGENTSTATUS("researcher_obscura")

  ' 5. Uso del browser (via plugin browser — il provider scelto è letto dal BrowserProviderRegistry)
  '    Il plugin browser (plugins/browser) userà Chromium o Obscura in base a:
  '    BrowserProviderRegistry.Resolve(agent.BrowserProvider) → IsAvailable ? launch
  '    Se obscura non è in cache (gmb setup OBSCURA non fatto), Resolve("obscura") ritorna
  '    comunque il provider ma IsAvailable=false → errore esplicito "gmb setup OBSCURA"
  '
  ' Esempio di navigazione (richiede browser avviato su porta 9222):
  ' AI AGENT ASK "researcher_obscura" "Naviga su https://example.com e riassumi il contenuto"

  ' 6. Reset ad auto (sceglie obscura se cached, altrimenti chromium)
  AI AGENT SET "researcher_obscura" BROWSER %AUTO
  PRINT "reset auto OK"
  PRINT "dopo reset browser="; AI.AGENTGET("researcher_obscura", "browser")

  ' 7. Setup verifica (da shell, non da BASIC):
  '    gmb setup BROWSER   — scarica Chromium (Chrome for Testing)
  '    gmb setup OBSCURA   — scarica Obscura (Rust+V8)
  '    gmb setup status    — mostra cache

  PRINT "Sample BrowserProvider per-agent completato."

  AI AGENT DELETE "researcher_chrome"
  AI AGENT DELETE "researcher_obscura"
END SUB
