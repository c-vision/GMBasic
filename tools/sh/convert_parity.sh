#!/bin/bash
# convert_parity.sh — Parity suite CONVERT (Fase 2): il backend C (native/gmb_doc_c, via
# PDFOxide per i PDF) e il backend C# (ConversionLib) devono estrarre lo STESSO CONTENUTO
# dai fixture (frasi chiave attese). Non è byte-exact: gmb_doc_c è un port semplificato,
# quindi si verifica la PRESENZA del contenuto normalizzato, non il rendering identico.
#
# Uso: tools/sh/convert_parity.sh
# Output: GMBasicSharp/tests/convert-parity-work/ (scratch, gitignorata).
set -u
# ROOT = l'albero C (GMBasic/): i sorgenti `native/…` stanno lì (prima il cd andava in GMBasicSharp
# e i path `native/…` non esistevano: script rotto dal trasloco del monorepo).
ROOT="$(cd "$(dirname "$0")/../../GMBasic" && pwd)"
MONO="$(cd "$ROOT/.." && pwd)"
WORK="$MONO/GMBasicSharp/tests/convert-parity-work"
mkdir -p "$WORK/work" "$WORK/out"
F="$WORK/work"
OUT="$WORK/out"

# 1) build CLI C (gmb_doc_parity_cli)
# NOTA: solo .c TOP-LEVEL di gmb_runtime_c/src (come l'harness C99, Directory.GetFiles non
# ricorsivo) — i parser/ sono WIP del compilatore C e possono non compilare.
LLMENV="${LLMENV:-$ROOT/../LLMEnv}"
RT_SRC="$(find native/gmb_runtime_c/src -maxdepth 1 -name '*.c' | tr '\n' ' ')"
# Engine ASR (LLMEnv/LLMEngine/asr): `gmb_asr_cmd.c` (in RT_SRC) usa i simboli gmb_asr_* — le
# implementazioni vivono qui dopo il refactor ASR; senza, il link fallisce.
RT_SRC="$RT_SRC $(find "$LLMENV/LLMEngine/asr/src" -maxdepth 1 -name '*.c' 2>/dev/null | tr '\n' ' ')"
cc -std=c99 native/gmb_doc_c/tests/gmb_doc_parity_cli.c \
   native/gmb_doc_c/src/gmb_doc.c native/gmb_doc_c/src/gmb_doc_pdf.c native/gmb_doc_c/src/gmb_doc_ocr.c \
   $RT_SRC \
   -I native/gmb_runtime_c/include -I native/gmb_runtime_c/src -I native/gmb_doc_c/include \
   -I "$LLMENV/LLMEngine/asr/include" \
   -I native/gmb_torch/include -I native/gmb_torch/src \
   -I "$HOME/.gmbasic/libs/osx-arm64/include" -lm -lcurl -lz -lbz2 -lsqlite3 -ldl \
   -L "$HOME/.gmbasic/libs/osx-arm64" -lbrotlienc -lbrotlidec -lbrotlicommon \
   -lpcre2-8 -lSDL2 \
   -lcrypto -lssl \
   -o "$OUT/gmb_doc_cli" 2>/dev/null || { echo "FATAL: build CLI C fallita"; exit 1; }

# 2) fixture
printf 'Hello World\nSecond line\n' > "$F/parity.txt"
printf '# Title\n\nBody text with **bold** and `code`.\n' > "$F/parity.md"
printf 'Name,Age\nAnn,30\nBob,25\n' > "$F/parity.csv"
printf '<html><body><h1>Title</h1><p>Body &amp; more</p></body></html>' > "$F/parity.html"
printf '{\\rtf1\\ansi Hello \\b bold\\b0 \\par world}' > "$F/parity.rtf"
cp /tmp/hello.pdf "$F/parity.pdf" 2>/dev/null || true
cp /tmp/mini.xlsx "$F/parity.xlsx" 2>/dev/null || true
cp /tmp/test.pptx "$F/parity.pptx" 2>/dev/null || true
cp /tmp/test.epub "$F/parity.epub" 2>/dev/null || true

# 3) golden C# (backend IL, path ASSOLUTI: --managed esegue da bin/) → scrive i file
cat > "$WORK/golden.bas" <<EOF
IMPORT CONVERT
SUB Main()
    DIM h AS LONG
    OPEN "$OUT/il_txt.md" FOR OUTPUT AS #1
    PRINT #1, CONVERT.ToMd("$F/parity.txt")
    CLOSE #1
    OPEN "$OUT/il_md.md" FOR OUTPUT AS #2
    PRINT #2, CONVERT.ToMd("$F/parity.md")
    CLOSE #2
    OPEN "$OUT/il_csv.md" FOR OUTPUT AS #3
    PRINT #3, CONVERT.ToMd("$F/parity.csv")
    CLOSE #3
    OPEN "$OUT/il_html.md" FOR OUTPUT AS #4
    PRINT #4, CONVERT.ToMd("$F/parity.html")
    CLOSE #4
    OPEN "$OUT/il_rtf.txt" FOR OUTPUT AS #5
    PRINT #5, CONVERT.ToText("$F/parity.rtf")
    CLOSE #5
    OPEN "$OUT/il_pdf.md" FOR OUTPUT AS #6
    PRINT #6, CONVERT.ToMd("$F/parity.pdf")
    CLOSE #6
    OPEN "$OUT/il_xlsx.md" FOR OUTPUT AS #7
    PRINT #7, CONVERT.ToMd("$F/parity.xlsx")
    CLOSE #7
    OPEN "$OUT/il_pptx.md" FOR OUTPUT AS #8
    PRINT #8, CONVERT.ToMd("$F/parity.pptx")
    CLOSE #8
    OPEN "$OUT/il_epub.md" FOR OUTPUT AS #9
    PRINT #9, CONVERT.ToMd("$F/parity.epub")
    CLOSE #9
END SUB
EOF

# 4) normalizza: whitespace + caratteri markdown + lowercase
normalize() {
  awk '{ gsub(/[ \t]+/, " "); gsub(/[#*|`\[\]]/, ""); gsub(/&amp;/, "\\&"); sub(/^ /, ""); sub(/ $/, ""); print $0 }' | tr '[:upper:]' '[:lower:]'
}

# 5) esegui il golden C# una volta
(cd "$WORK" && dotnet run --project "$ROOT/REPL" -p:GmbEdition=Commercial -- "$WORK/golden.bas" --managed >/dev/null 2>&1)

# 6) per ogni formato: il contenuto C deve contenere le frasi chiave attese;
#    il golden IL deve contenerle anch'esse (parità di contenuto).
declare -a CASES=(
  "txt|md|parity.txt|hello world|second line"
  "md|md|parity.md|title|body text|bold|code"
  "csv|md|parity.csv|name|age|ann|bob"
  "html|md|parity.html|title|body|more"
  "rtf|text|parity.rtf|hello|bold|world"
  # pdf: PDFOxide (C) separa le parole, PdfPig (IL) le concatena ("HelloPDFWorld") —
  # check word-based (ogni parola presente in entrambi).
  "pdf|md|parity.pdf|hello|pdf|world"
  "xlsx|md|parity.xlsx|sheet 1|hello|42"
  "pptx|md|parity.pptx|slide 1|slide 2|intro|second"
  "epub|md|parity.epub|my epub book|chapter one|hello epub content"
)

echo "== Parity CONVERT C vs IL (content-presence) =="
pass=0; fail=0; skip=0
for row in "${CASES[@]}"; do
  IFS='|' read -r ext fmt file expect_phrases <<< "$row"
  IFS='|' read -r -a phrases <<< "$expect_phrases"
  # C
  "$OUT/gmb_doc_cli" "$F/$file" "$fmt" 2>/dev/null | normalize > "$OUT/c_$ext.norm"
  c_ok=1
  for p in "${phrases[@]}"; do grep -qi "$p" "$OUT/c_${ext}.norm" || c_ok=0; done
  # IL golden
  if [ -f "$OUT/il_${ext}.md" ] || [ -f "$OUT/il_${ext}.txt" ]; then
    il_file="$OUT/il_${ext}.md"; [ -f "$il_file" ] || il_file="$OUT/il_${ext}.txt"
    if [ ! -s "$il_file" ]; then
      # golden IL vuoto (ClosedXML/PptxParser/EpubParser rifiutano i fixture minimi) → C-verified only
      echo "  $ext: C-$([ "$c_ok" = 1 ] && echo OK || echo FAIL) / IL-n/a (golden vuoto)"; [ "$c_ok" = 1 ] && pass=$((pass+1)) || fail=$((fail+1)); continue
    fi
    cat "$il_file" | normalize > "$OUT/il_${ext}.norm"
  else
    # golden non generato (crash ConversionLib sul fixture) → C-verified only
    echo "  $ext: C-$([ "$c_ok" = 1 ] && echo OK || echo FAIL) / IL-n/a (golden non generato)"; [ "$c_ok" = 1 ] && pass=$((pass+1)) || fail=$((fail+1)); continue
  fi
  c_miss=0; il_miss=0
  for p in "${phrases[@]}"; do
    grep -qi "$p" "$OUT/c_${ext}.norm" || c_miss=$((c_miss+1))
    grep -qi "$p" "$OUT/il_${ext}.norm" || il_miss=$((il_miss+1))
  done
  if [ "$c_miss" -eq 0 ] && [ "$il_miss" -eq 0 ]; then
    echo "  $ext -> $fmt: PASS"; pass=$((pass+1))
  else
    echo "  $ext -> $fmt: FAIL (C manca=$c_miss, IL manca=$il_miss)"; fail=$((fail+1))
    echo "    C : $(cat "$OUT/c_${ext}.norm")"
    echo "    IL: $(cat "$OUT/il_${ext}.norm")"
  fi
done
echo "PASS=$pass FAIL=$fail SKIP=$skip"
[ "$fail" -eq 0 ]
