#!/usr/bin/env bash
# Starts the real server binary and checks deployment settings over HTTP:
# PORT, 0.0.0.0 binding, CORS origin, persistence across a restart, fail-fast startup.
# Usage: scripts/smoke_test.sh [path-to-chronoplan-binary]   (Linux/macOS; needs curl)
set -euo pipefail
BIN="${1:-build/chronoplan}"
TMP="$(mktemp -d)"; PORT_N=$((20000 + RANDOM % 20000)); PID=""
cleanup() { [[ -n "$PID" ]] && kill "$PID" 2>/dev/null || true; rm -rf "$TMP"; }
trap cleanup EXIT
fail() { echo "SMOKE FAIL: $*"; exit 1; }
start() { env CHRONOPLAN_DB_PATH="$TMP/data/s.db" CHRONOPLAN_BIND_ADDR=0.0.0.0 PORT=$PORT_N \
          CHRONOPLAN_CORS_ORIGIN=https://app.example.com "$BIN" >"$TMP/log" 2>&1 & PID=$!; sleep 1; }
URL="http://127.0.0.1:$PORT_N/api/v1"

start
grep -q "Listening on 0.0.0.0:$PORT_N" "$TMP/log" || fail "bind/PORT not applied: $(cat "$TMP/log")"
curl -s "$URL/schedule/optimize/health" | grep -q online || fail "health"
curl -si "$URL/tasks" | grep -qi "Access-Control-Allow-Origin: https://app.example.com" || fail "CORS origin"
curl -s -X POST "$URL/tasks" -H 'Content-Type: application/json' -d '{"id":7,"title":"t","duration_min":30}' | grep -q '"id":7' || fail "create"
kill "$PID"; wait "$PID" 2>/dev/null || true; PID=""
start
curl -s "$URL/tasks/7" | grep -q '"title":"t"' || fail "data lost after restart"
kill "$PID"; wait "$PID" 2>/dev/null || true; PID=""

# invalid port must abort startup, not fall back silently
set +e; PORT=99999 CHRONOPLAN_DB_PATH="$TMP/data/s.db" "$BIN" >"$TMP/log2" 2>&1; rc=$?; set -e
[[ $rc -eq 3 ]] || fail "invalid PORT should exit 3, got $rc"
echo "SMOKE OK"
