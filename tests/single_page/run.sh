#!/usr/bin/env bash
# Drive the single-page smoke test:
#   1. start the binary on a random port
#   2. wait for it to listen
#   3. curl each route, check status + body content
#   4. tear down on exit
set -uo pipefail

BINARY="${1:?usage: run.sh <binary> <static_dir>}"
STATIC_DIR="${2:?usage: run.sh <binary> <static_dir>}"

PORT=$((10000 + RANDOM % 50000))
URL="http://127.0.0.1:$PORT"

"$BINARY" "$PORT" "$STATIC_DIR/pico.min.css" >/dev/null 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true; wait "$SERVER_PID" 2>/dev/null || true' EXIT

# Wait up to ~5s for the acceptor to be ready.
for _ in $(seq 1 50); do
    if curl -fsS -o /dev/null "$URL/" 2>/dev/null; then break; fi
    sleep 0.1
done

fail=0
check() {
    local desc="$1" path="$2" expect="$3"
    local response status body
    if ! response=$(curl -sS -w $'\n%{http_code}' "$URL$path" 2>/dev/null); then
        echo "FAIL [$desc] curl could not reach $URL$path"
        fail=1
        return
    fi
    status="${response##*$'\n'}"
    body="${response%$'\n'*}"
    if [[ "$status" != "200" ]]; then
        echo "FAIL [$desc] expected 200, got $status"
        fail=1
    elif ! grep -q -- "$expect" <<< "$body"; then
        echo "FAIL [$desc] body did not contain '$expect'"
        fail=1
    else
        echo "PASS [$desc]"
    fi
}

check "/"                    "/"                    "Hello, world!"
check "/hello/:name"         "/hello/fox"           "Hello, fox!"
check "cpp-for renders item" "/"                    "writev zero-copy"
check "/static/pico.min.css" "/static/pico.min.css" "Pico CSS"

exit $fail
