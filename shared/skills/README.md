# GMBASIC — Archivio Skills Agente

Cartella di default con le **skills** disponibili all'agente AI di GMBASIC.

- **161 skill** in formato markdown, ognuna in `skills/<nome>/SKILL.md` (+ 20 skill dal
  marketplace configurato in `config/ai.json` → **181** in `SKILLS_PROMPT.md`)
- **`SKILLS_PROMPT.md`** — l'**indice** (istruzioni all'agente + elenco delle skill con la
  descrizione). È un **output generato**: `AI SKILLS REFRESH` / `AI.SKILLSPROMPT()` lo
  riscrivono in `<configDir>/SKILLS_PROMPT.md` (accanto ad `ai.json`), **non** qui dentro —
  l'archivio resta contenuto puro e può essere in sola lettura (programma installato).
  Il testo che l'agente deve leggere è comunque il **valore di ritorno** di
  `AI.SKILLSPROMPT()`, non il file.

## Origine

Le skill sono state selezionate dalle installazioni locali dei tool AI:
Claude Code, Codex, opencode, Gemini, `~/.agents/skills` (GSD/utility).
Priorità ai domini utili a un **agente in un programma compilato**: finanza/trading,
email, documenti/PDF, business, dati, media, web, security, produttività.

## Come usare le skill

1. L'agente legge `SKILLS_PROMPT.md` per conoscere l'indice delle skill.
2. Quando una richiesta corrisponde a una skill, l'agente legge
   `skills/<nome>/SKILL.md` e ne segue le istruzioni.
3. L'utente può aggiungere altre skill da BASIC (file markdown nella stessa cartella).

## Categorie

| Categoria | n. | Esempi |
|---|---|---|
| Finanza / trading / investimenti | 25 | financial-analyst, fund-screener, commercial-forecaster, pricing-strategy |
| Email | 12 | gmail-inbox-triage, outlook-email, email-template-builder |
| Documenti / PDF / Office | 14 | contract-review, docx, pdf, spreadsheets, invoice-chase |
| Business / strategia | 36 | market-research, board-prep, content-strategy, crm |
| Dati / analisi | 9 | data-visualization, dashboard-expert, statistical-analyst |
| Media | 6 | video, image, social-media-manager |
| Web / integrazione | 12 | browser-automation, browser-skill, mcp-integration, web-search |
| Security / compliance | 8 | gdpr-audit-prep, ai-security, soc2-audit-prep |
| Produttività / altro | 27 | gsd-*, goose-*, hallmark, impeccable |

*(i conteggi si riferiscono al momento della generazione di `SKILLS_PROMPT.md`)*
