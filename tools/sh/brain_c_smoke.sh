#!/usr/bin/env bash
# brain_c_smoke.sh — smoke test del Second Brain in C (`gmb brain …`).
#
# Copre ciò che prima verificavano i test C# del toolchain (rimossi con F9d):
# create · stats · verify · sign · export · install · use/detach · dedupe · extract ·
# merge · convert · diff · doctor · rename · export-obsidian.
# Isola la configurazione in una HOME temporanea (nessun effetto sull'utente reale).
#
# Uso:  tools/sh/brain_c_smoke.sh [path-del-binario-gmb]
set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
GMB="${1:-$REPO/GMBasic/native/gmb_compiler_c/build/gmb}"
if [ ! -x "$GMB" ]; then
  echo "?gmb non trovato: $GMB (compila GMBasic/native/gmb_compiler_c)" >&2
  exit 1
fi

W="$(mktemp -d "${TMPDIR:-/tmp}/gmb-brain-smoke.XXXXXX")"
export HOME="$W/home"
# GMBASIC_ROOT (legacy da harness) isola config/rag/graphs/brains in una root usa-e-getta:
# nessuna scrittura nella config reale dell'utente o del checkout.
export GMBASIC_ROOT="$W/root"
mkdir -p "$HOME" "$GMBASIC_ROOT/config"
# paths.* (options, non env): rag/graphs/brains dentro la root temporanea
cat > "$GMBASIC_ROOT/config/system.settings.json" <<JSON
{
  "paths": {
    "ragDir": "$W/root/rag",
    "graphsDir": "$W/root/graphs",
    "brainsDir": "$W/root/brains",
    "configDir": "$W/root/config"
  }
}
JSON
trap 'rm -rf "$W"' EXIT

FAIL=0
ok()   { printf '  ok   %s\n' "$1"; }
bad()  { printf '  FAIL %s\n' "$1"; FAIL=$((FAIL+1)); }
run()  { "$@" >"$W/out.txt" 2>"$W/err.txt"; return $?; }
has()  { grep -q -- "$1" "$W/out.txt" || { bad "$2 (output: $(head -3 "$W/out.txt" | tr '\n' ' '))"; return 1; }; ok "$2"; return 0; }

# vault di prova
mkdir -p "$W/vault/sub"
printf '# Alpha\n[[Beta]] [[sub/Gamma|alias]]\n' > "$W/vault/Alpha.md"
printf '# Beta\n[[Alpha]]\n' > "$W/vault/Beta.md"
printf '# Gamma\ntesto\n' > "$W/vault/sub/Gamma.md"

echo "== brain: registry e toolchain (C) =="

run "$GMB" brain create sm1 --from "$W/vault" --mode graph --register && has "GRAPH:" "create graph (nodi/edge)" || bad "brain create (exit $?)"

run "$GMB" brain info sm1 && has "mode:" "info" || bad "brain info"
run "$GMB" brain stats sm1 && has "BRAIN STATS" "stats" || bad "brain stats"
run "$GMB" brain verify sm1 && has ": OK" "verify" || bad "brain verify"
run "$GMB" brain stats sm1 --json && has '"graph"' "stats --json" || bad "stats --json"

# export / import (C)
ZIP="$W/sm1.gmbrain.zip"
run "$GMB" brain export sm1 --out "$ZIP" && [ -f "$ZIP" ] && ok "export .gmbrain.zip" || bad "brain export"
python3 - "$ZIP" <<'PY' || FAIL=$((FAIL+1))
import sys, zipfile, json
z = zipfile.ZipFile(sys.argv[1])
m = json.loads(z.read('brain.manifest.json'))
assert m["format"] == "gmbrain" and m["files"], "manifest mancante/incompleto"
assert m["checksum"].startswith("sha256:")
print("  ok   manifest (format gmbrain, files, checksum)")
PY

# sign di una cartella utente
run "$GMB" brain sign sig1 "$W/vault" && has "firmato" "sign (probe graph)" || bad "brain sign"
run "$GMB" brain verify sig1 && has ": OK" "verify del firmato" || bad "verify sig1"

# use / detach / list / forget
run "$GMB" brain use sig1 && ok "use" || bad "brain use"
run "$GMB" brain detach sig1 && ok "detach" || bad "brain detach"
run "$GMB" brain list && has "sig1" "list" || bad "brain list"
run "$GMB" brain forget sig1 && ok "forget" || bad "brain forget"

# rename
run "$GMB" brain rename sm1 sm2 && ok "rename" || bad "brain rename"
run "$GMB" brain info sm2 && has "sm2" "info dopo rename" || bad "info dopo rename"

# optimize / dedupe / diff / doctor (artefatti solo-grafo)
run "$GMB" brain optimize sm2 && has "GRAPH:" "optimize (graph)" || bad "brain optimize"
run "$GMB" brain dedupe sm2 && has "GRAPH:" "dedupe report" || bad "brain dedupe"
run "$GMB" brain create sm3 --from "$W/vault" --mode graph --register && ok "create sm3" || bad "create sm3"
run "$GMB" brain diff sm2 sm3 && has "BRAIN DIFF" "diff" || bad "brain diff"
run "$GMB" brain doctor sm2 && has "BRAIN DOCTOR" "doctor" || bad "brain doctor"

# graph export-obsidian
run "$GMB" graph export-obsidian sm2 --out "$W/obs" && [ -d "$W/obs" ] && ok "graph export-obsidian" || bad "graph export-obsidian"

# convert rag→graph su un db RAG sintetico (nessun provider embedding richiesto)
python3 - "$W/r1.db" <<'PY' || FAIL=$((FAIL+1))
import sqlite3, struct, sys
db = sqlite3.connect(sys.argv[1])
db.executescript("""
CREATE TABLE vec_collections (id INTEGER PRIMARY KEY, name TEXT UNIQUE, dimensions INTEGER, metric TEXT, model_version TEXT DEFAULT 'fixture-v1', model_digest TEXT, format TEXT DEFAULT 'fp32', created_at TEXT DEFAULT (datetime('now')));
CREATE TABLE vec_documents (id INTEGER PRIMARY KEY, collection_id INTEGER, source_id TEXT, source_hash TEXT, status TEXT DEFAULT 'current', tenant_id TEXT, access_roles TEXT, metadata_json TEXT, created_at TEXT DEFAULT (datetime('now')), updated_at TEXT DEFAULT (datetime('now')), UNIQUE(collection_id, source_id));
CREATE TABLE vec_chunks (id INTEGER PRIMARY KEY, document_id INTEGER, chunk_index INTEGER, text TEXT, token_count INTEGER, metadata_json TEXT, UNIQUE(document_id, chunk_index));
CREATE TABLE vec_embeddings (chunk_id INTEGER, model_version TEXT, embedding_created_at TEXT DEFAULT (datetime('now')), dimensions INTEGER, embedding BLOB, PRIMARY KEY (chunk_id, model_version));
CREATE TABLE fts_chunks (text, chunk_id UNINDEXED, collection_id UNINDEXED);
INSERT INTO vec_collections(id,name,dimensions,metric) VALUES(1,'wiki',4,'cosine');
""")
for i,(sid,txt) in enumerate([("a.md","link [[b]]"),("b.md","link [[a]]")], start=1):
    db.execute("INSERT INTO vec_documents(id,collection_id,source_id,source_hash) VALUES(?,1,?,?)", (i,sid,'h%d'%i))
    db.execute("INSERT INTO vec_chunks(document_id,chunk_index,text) VALUES(?,0,?)", (i,txt))
    cid = db.execute("SELECT last_insert_rowid()").fetchone()[0]
    db.execute("INSERT INTO vec_embeddings(chunk_id,model_version,dimensions,embedding) VALUES(?,?,4,?)",
               (cid,'fixture-v1',struct.pack('<4f',0.1,0.2,0.3,0.4)))
    db.execute("INSERT INTO fts_chunks(text,chunk_id,collection_id) VALUES(?,?,1)", (txt,cid))
db.commit(); db.close()
print("  ok   db RAG sintetico")
PY
run "$GMB" brain register r1 --mode rag --source "$W/r1.db" && ok "register rag" || bad "brain register"
run "$GMB" brain stats r1 && has "rag:" "stats rag" || bad "stats r1"
run "$GMB" brain verify r1 && has ": OK" "verify r1" || bad "verify r1"
run "$GMB" brain dedupe r1 && has "RAG:" "dedupe rag (report)" || bad "dedupe r1"
run "$GMB" brain extract r1 --out r1sub --source a && has "RAG:" "extract rag" || bad "extract r1"
run "$GMB" brain merge r1 r1sub --out rmerge && has "RAG:" "merge rag" || bad "merge"
run "$GMB" brain convert r1 --to graph --out r1graph --min-edges 0 && has "CONVERT rag" "convert rag→graph" || bad "convert"

echo
if [ "$FAIL" -eq 0 ]; then
  echo "BRAIN C SMOKE: OK"
  exit 0
fi
echo "BRAIN C SMOKE: $FAIL FAIL"
exit 1
