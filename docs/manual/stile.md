# Regole editoriali del manuale (F2)

> Si applicano a ogni capitolo, didascalia, tabella e nota del libro. Il gate
> `tools/sh/check_manual_terms.sh` verifica le regole 1, 4, 5 e 6 su tutti i `.md` del libro.

## 1. Numeri canonici

| Cosa | Forma corretta | Forma vietata |
|---|---|---|
| Backend | **quattro**: IL, PCodeVM, VM/C, C99Native (in quest'ordine) | "cinque backend", "5 backend", "cinque gambe" |
| Strumento di parità | `parity4` | `parity5` |
| Funzioni della superficie | **1.540** | 982, "oltre millecinquecento" |
| Voci della reference | **2.069** (1.533 funzioni + 174 comandi + 78 tipi + 284 costanti) | "circa duemila" |
| Pagine | stima dichiarata nell'indice (corpo + appendici + reference), non arrotondata a caso | numeri diversi da quelli dell'indice |

I numeri si scrivono con il punto delle migliaia (1.540, 2.069), mai con la virgola.

## 2. Comandi e funzioni

- Un **comando** si scrive in maiuscolo, a spazio, **senza parentesi**: `DBF USE "f.dbf" ALIAS D`.
- Una **funzione** si scrive in maiuscolo con namespace e **parentesi**: `n = DBF.LASTREC()`.
- Il nome del namespace si separa con il punto solo nelle funzioni e nei metodi.
- Non esiste la forma qualificata di un comando: se nel testo compare `DBF.USE(...)`, è un errore.
- Le costanti di linguaggio si scrivono con il `%`: `%CHAT`, non `CHAT`.

## 3. Codice nel testo

- Codice in linea fra apici inversi: `WAITGROUP`.
- Blocchi di codice con linguaggio dichiarato (`basic`), una sola istruzione per riga, nessun
  carattere di continuazione inventato.
- Ogni esempio deve esistere come file reale e, dove possibile, come MiniTest con oracolo.
  Nel testo si cita il percorso relativo (`shared/samples/...`) accanto all'esempio.
- Output e messaggi di errore si riportano **come escono**, senza abbellirli.

## 4. Licenza

- GMBasic è distribuito sotto **PUL v1.1**: non è open source e non va mai chiamato così.
- Il perimetro di inferenza (LLMEnv, FastLLM) è **Apache-2.0**; la produzione dei pesi
  (`gmbllm`, namespace `TRAIN`) è **PUL**. Se il testo parla di modelli, la distinzione è esplicita.
- La pagina di copyright del libro riporta PUL e il rimando al file `LICENSE`.

## 5. Nomi e percorsi reali

- Si citano solo percorsi che esistono nel monorepo: `tools/sh/parity4`, `tools/sh/gmb_c99native_run.sh`,
  `shared/samples/`, `shared/minitests/`, `docs/manual/` (sede del libro).
- Percorsi morti da non citare: `docs/comparison/`, `docs/implemented/`,
  `docs/Sync-GMBasic-Monorepo.md`, `GMBasicSharp/common/C99Backend/`.
- `C99Backend` non esiste più come backend (ritirato il 2026-10-25): se serve nominarlo, è storia.
- `GMB_PKG_CACHE` è un residuo storico: la cache dei pacchetti si configura con le impostazioni.

## 6. Configurazione

- I parametri si impostano con il **registro delle opzioni** (`AI SET OPTION`, sezione `engine`
  delle impostazioni) o con i file di configurazione.
- Il manuale non insegna a configurare GMBasic con variabili d'ambiente, perché il progetto non
  le usa per il comportamento.

## 7. Lingua e tono

- Italiano, con i termini tecnici non tradotti (backend, runtime, park, handle) nella forma del glossario.
- "La GPM" al femminile, "il runtime", "un task GPM", "la gamba di parità".
- Voce impersonale o seconda persona singolare, scelta una volta per capitolo e mantenuta.
- Niente slogan, niente "potente", "rivoluzionario", "semplice e veloce": si descrive cosa fa.
- Un concetto, un nome: se il glossario dice *backend*, il testo non alterna *motore*, *engine*, *gamba*
  nello stesso paragrafo.

## 8. Struttura di ogni capitolo

1. Cosa si ottiene leggendolo (2-3 righe, nessun elenco di argomenti).
2. Il corpo, con esempi eseguiti.
3. Tabella dei simboli usati, con namespace e firma.
4. Errori frequenti, con il messaggio reale e la causa.
5. Rimandi ai capitoli collegati.

## 9. Checklist prima di consegnare un capitolo

- [ ] Nessuna forma vietata della regola 1 (il gate è verde).
- [ ] Ogni esempio esiste nel repo ed è stato eseguito.
- [ ] Ogni simbolo citato esiste nella reference (stesso nome, stesso namespace).
- [ ] Nessun percorso morto, nessuna variabile d'ambiente come configurazione.
- [ ] PUL e Apache nominati correttamente se il capitolo parla di licenza o di modelli.
- [ ] Termini allineati al glossario; se serve un termine nuovo, si aggiunge **prima** al glossario.
