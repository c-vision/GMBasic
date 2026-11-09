/* gmb_ns.h — namespace builtin noti (derivati da StdLib.BuiltinNamespaceNames).
 * Usato dal parser per risolvere NS.FUNZIONE → FUNZIONE (la qualifica non e' mai
 * dispatchata; l'emitter riceve il nome nudo). Generato da tools/audit --namespaces.
 *
 * ALLINEAMENTO 2026-10-29: la lista era STALE rispetto a StdLib.BuiltinNamespaceNames
 * (il riferimento): mancavano ASR, CONTROLS e GREP — tutti e tre registrati nel C#
 * (`StdLib.Controls.cs` -> ns "CONTROLS"; `StdLib.AI`/`StdLib.Grep` per ASR/GREP) e
 * presenti nei 69 namespace del registry canonico (shared/spec/registry).
 * Conseguenza: `IMPORT CONTROLS` nei moduli di shared/Controls/*.bas veniva rifiutato con
 * "nessun modulo con NAMESPACE CONTROLS (né namespace builtin)" anche se NESSUN file
 * .inc/.bas dichiara quel namespace: CONTROLS è builtin, non un modulo da trovare su disco.
 * Rigenerare con `tools/audit --namespaces` appena l'audit compila (riferimenti di progetto
 * da ripuntare, vedi tools/audit/README.md): queste tre voci sono quelle che il generatore
 * deve riemettere. */
#ifndef GMB_NS_H
#define GMB_NS_H

static const char* const gmb_ns_names[] = {
    "AI",
    "ANALYSIS",
    "ARRAYS",
    "ASR",
    "ATOMIC",
    "AUTH",
    "BLOCKCHAIN",
    "CHART",
    "CLIP",
    "COLLECTIONS",
    "COMPRESSION",
    "CONCURRENCY",
    "CONSOLE",
    "CONSTANTS",
    "CONTEXT",
    "CONTROLS",
    "CONVERSIONS",
    "CONVERT",
    "CRYPTOGRAPHY",
    "DATETIME",
    "DBF",
    "GRAPHICS",
    "GREP",
    "HTTP",
    "IO",
    "ISAM",
    "JOY",
    "JSON",
    "KDTREE",
    "LEDGER",
    "MATH",
    "MATRIX",
    "MEMORYMAP",
    "MOUSE",
    "MUTEX",
    "NET",
    "ONKEY",
    "OPTIMIZE",
    "POOL",
    "PROVIDER",
    "RDD",
    "SIGNAL",
    "SPATIAL",
    "SPECIAL",
    "SPEED",
    "SQL",
    "STATS",
    "STRINGS",
    "SYNCMAP",
    "SYSTEM",
    "TIMESERIES",
    "TORCH",
    "TRADING",
    "TRADING.BACKTESTS",
    "TRADING.BONDS",
    "TRADING.METRICS",
    "TRADING.OPTIONS",
    "TRADING.PORTFOLIO",
    "TYPEINFO",
    "VECTOR",
    "VECTORDB",
    "WINDOWS",
    /* Second Brain (2026-09-01): namespace autonomi AI/GRAPH/VAULT/NOTE/INGEST. */
    "GRAPH",
    "BRAINS",
    "VAULT",
    "NOTE",
    "INGEST",
    "AGENT",
    "BROWSER",
    "COMPUTER",
    "TOOL",
    "UI",
    "LLM",
    "TRAY",
    "TUI",
};
#define GMB_NS_COUNT (sizeof(gmb_ns_names)/sizeof(gmb_ns_names[0]))
#endif
