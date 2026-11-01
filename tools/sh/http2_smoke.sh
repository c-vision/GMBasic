#!/usr/bin/env bash
# tools/sh/http2_smoke.sh — verifica VIVA di HTTP/2 sul server C (C99Native).
#
# Perché serve: HTTP/2 non si può provare da BASIC (il client BASIC parla HTTP/1.1) e i
# MiniTest coprono solo HTTP/1.1. Qui si avvia un vero server nativo e si interroga con curl:
#   h2c (prior knowledge), fallback HTTP/1.1, Range 206, 304, compressione br, HEAD.
#
# Uso:  tools/sh/http2_smoke.sh [porta]
# Exit: 0 = tutti i check PASS, 1 = almeno un FAIL.
set -uo pipefail
cd "$(dirname "$0")/../.." || exit 2

PORT="${1:-8781}"
SAMPLES_DIR="shared/samples/Http"
BIN_DIR="${TMPDIR:-/tmp}/gmb_http2_smoke_$$"
PROG="$BIN_DIR/h2srv.bas"
LOG="$BIN_DIR/server.log"
PASS=0; FAIL=0
ok()   { PASS=$((PASS+1)); printf '  \033[32mPASS\033[0m %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); printf '  \033[31mFAIL\033[0m %s — %s\n' "$1" "${2:-}"; }
cleanup() {
  [ -n "${SRV_PID:-}" ] && kill "$SRV_PID" 2>/dev/null
  lsof -ti ":$PORT" 2>/dev/null | xargs kill -9 2>/dev/null
  rm -rf "$BIN_DIR"
}
trap cleanup EXIT

mkdir -p "$BIN_DIR/static"
python3 - "$BIN_DIR/static/big.txt" <<'PY'
import sys
open(sys.argv[1], "w").write("A" * 70000)
PY

cat > "$PROG" <<EOF
IMPORT HTTP
SUB AuthMw(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS LONG)
  IF req.Headers.Get("X-Token") = "ok" THEN
    HTTP USE NEXT
  ELSE
    res.SetStatus(401)
    res.Send("no-token")
  END IF
END SUB
SUB Hello(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("hello-h2")
END SUB
SUB Sse(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.StartStream("text/event-stream")
  res.Write("data: uno" + CHR\$(10) + CHR\$(10))
  res.Flush()
  SLEEP 150
  res.Write("data: due" + CHR\$(10) + CHR\$(10))
  res.Flush()
  res.EndStream()
END SUB
SUB Echo(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("len=" + LTRIM\$(STR\$(LEN(req.Body))))
END SUB
SUB Main()
  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = $PORT
  cfg.MaxRequestBodySize = 1000
  HTTP CONFIG cfg
  HTTP USE "/api/", AuthMw()
  HTTP ROUTE "GET", "/hello", Hello()
  HTTP ROUTE "GET", "/api/hello", Hello()
  HTTP ROUTE "GET", "/sse", Sse()
  HTTP ROUTE "POST", "/echo", Echo()
  HTTP STATIC "/static", "$BIN_DIR/static"
  PRINT "pronto"
  HTTP SERVER $PORT
  SLEEP 60000
END SUB
EOF

echo "== HTTP/2 smoke (C99Native, porta $PORT) =="
lsof -ti ":$PORT" 2>/dev/null | xargs kill -9 2>/dev/null
( timeout 120 tools/sh/gmb_c99native_run.sh "$PROG" h2smoke >"$LOG" 2>&1 & echo $! >"$BIN_DIR/pid" )
SRV_PID="$(cat "$BIN_DIR/pid")"

for _ in $(seq 1 40); do
  curl -s -o /dev/null --max-time 1 "http://127.0.0.1:$PORT/hello" && break
  sleep 1
done

# 1) HTTP/2 h2c
OUT="$(curl -s --http2-prior-knowledge --max-time 8 -w '|%{http_version}' "http://127.0.0.1:$PORT/hello" 2>/dev/null)"
case "$OUT" in
  hello-h2\|2) ok "h2c: $OUT";;
  *) bad "h2c" "$OUT";;
esac

# 2) fallback HTTP/1.1 sulla stessa porta
OUT="$(curl -s --http1.1 --max-time 8 -w '|%{http_version}' "http://127.0.0.1:$PORT/hello" 2>/dev/null)"
case "$OUT" in
  hello-h2\|1.1) ok "http/1.1: $OUT";;
  *) bad "http/1.1" "$OUT";;
esac

# 3) file grande in h2 (flow control: 70 KB > finestra iniziale di 65535)
SZ="$(curl -s --http2-prior-knowledge -o "$BIN_DIR/o.bin" -w '%{size_download}' --max-time 10 "http://127.0.0.1:$PORT/static/big.txt" 2>/dev/null)"
[ "$SZ" = "70000" ] && ok "h2 file 70 KB intero (flow control)" || bad "h2 file 70 KB" "size=$SZ"

# 4) Range 206 in h2
code="$(curl -s --http2-prior-knowledge -o "$BIN_DIR/r.txt" -w '%{http_code}' --max-time 8 -H 'Range: bytes=0-9' "http://127.0.0.1:$PORT/static/big.txt" 2>/dev/null)"
[ "$code" = "206" ] && [ "$(wc -c < "$BIN_DIR/r.txt" | tr -d ' ')" = "10" ] && ok "h2 Range 206 (10 byte)" || bad "h2 Range" "code=$code"

# 5) 304 con ETag in h2
ET="$(curl -s --http2-prior-knowledge -D - -o /dev/null --max-time 8 "http://127.0.0.1:$PORT/static/big.txt" 2>/dev/null | tr -d '\r' | awk 'tolower($1)=="etag:"{print $2}')"
code="$(curl -s --http2-prior-knowledge -o /dev/null -w '%{http_code}' --max-time 8 -H "If-None-Match: $ET" "http://127.0.0.1:$PORT/static/big.txt" 2>/dev/null)"
[ -n "$ET" ] && [ "$code" = "304" ] && ok "h2 304 (ETag $ET)" || bad "h2 304" "code=$code etag=$ET"

# 6) brotli negoziato in h2
OUT="$(curl -s --http2-prior-knowledge -D - -o "$BIN_DIR/br.bin" --max-time 8 -H 'Accept-Encoding: br' "http://127.0.0.1:$PORT/static/big.txt" 2>/dev/null | tr -d '\r' | awk 'tolower($1)=="content-encoding:"{print $2}')"
SZ="$(wc -c < "$BIN_DIR/br.bin" | tr -d ' ')"
[ "$OUT" = "br" ] && [ "$SZ" -lt 100 ] && ok "h2 br (14 byte su 70 KB)" || bad "h2 br" "enc=$OUT size=$SZ"

# 7) HEAD in h2: header, nessun corpo
OUT="$(curl -s --http2-prior-knowledge -I --max-time 8 "http://127.0.0.1:$PORT/static/big.txt" 2>/dev/null | tr -d '\r')"
case "$OUT" in
  *"HTTP/2 200"*"content-length: 70000"*) ok "h2 HEAD (content-length 70000, nessun corpo)";;
  *) bad "h2 HEAD" "$(printf '%s' "$OUT" | head -3)";;
esac

# 8) middleware (HTTP USE) applicato anche su h2: 401 senza token, 200 con token
code="$(curl -s --http2-prior-knowledge -o /dev/null -w '%{http_code}' --max-time 8 "http://127.0.0.1:$PORT/api/hello" 2>/dev/null)"
[ "$code" = "401" ] && ok "h2 middleware: 401 senza token" || bad "h2 middleware 401" "code=$code"
OUT="$(curl -s --http2-prior-knowledge --max-time 8 -H 'X-Token: ok' "http://127.0.0.1:$PORT/api/hello" 2>/dev/null)"
[ "$OUT" = "hello-h2" ] && ok "h2 middleware: passa con X-Token" || bad "h2 middleware pass" "$OUT"

# 9) STREAMING (SSE) su h2: i due eventi arrivano in momenti diversi, senza framing chunked
T0=$(date +%s%N)
BODY="$(curl -s --http2-prior-knowledge -N --max-time 10 "http://127.0.0.1:$PORT/sse" 2>/dev/null)"
T1=$(date +%s%N)
MS=$(( (T1 - T0) / 1000000 ))
EXPECTED=$'data: uno\n\ndata: due'
if [ "$BODY" = "$EXPECTED" ] && [ "$MS" -ge 150 ]; then
  ok "h2 SSE in streaming (${MS}ms, corpo pulito)"
else
  bad "h2 SSE" "${MS}ms body=$(printf '%s' "$BODY" | head -2 | tr '\n' '|')"
fi

# 10) 413: corpo oltre MaxRequestBodySize (sia h2 sia h1)
BODY2000="$(python3 -c 'print("x"*2000, end="")')"
code="$(curl -s --http2-prior-knowledge -o /dev/null -w '%{http_code}' --max-time 8 -X POST --data-binary "$BODY2000" "http://127.0.0.1:$PORT/echo" 2>/dev/null)"
[ "$code" = "413" ] && ok "h2 413 oltre MaxRequestBodySize" || bad "h2 413" "code=$code"
code="$(curl -s --http1.1 -o /dev/null -w '%{http_code}' --max-time 8 -X POST --data-binary "$BODY2000" "http://127.0.0.1:$PORT/echo" 2>/dev/null)"
[ "$code" = "413" ] && ok "http/1.1 413 oltre MaxRequestBodySize" || bad "http/1.1 413" "code=$code"

echo ""
echo "──────────────────────────────────"
echo "  PASS: $PASS   FAIL: $FAIL"
[ "$FAIL" = "0" ] || exit 1
echo "  http2 smoke OK"
