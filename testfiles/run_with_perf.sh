#!/bin/bash
# Exit or error, treat undefined variables as errors.
# Pipeline fails if any command in it fails, not just the last one.
set -euo pipefail

TEST_NAME="$1"
TEST_BIN="$2"
FLAMEGRAPH_DIR="$3"
REPORT_DIR="$4"
shift 4

PERF_DATA="${REPORT_DIR}/perf.data"
FLAME_GRAPH_SVG="${REPORT_DIR}/${TEST_NAME}.svg"

STACKCOLLAPSE="${FLAMEGRAPH_DIR}/stackcollapse-perf.pl"
FLAMEGRAPH="${FLAMEGRAPH_DIR}/flamegraph.pl"

# Sanity checks
if [[ ! -f "$STACKCOLLAPSE" || ! -f "$FLAMEGRAPH" ]]; then
    echo "Error: FlameGraph scripts not found in ${FLAMEGRAPH_DIR}"
    exit 1
fi

echo "[perf] Running test: ${TEST_NAME}"

# Run the test with perf record
perf record -o "$PERF_DATA" -e cpu-clock:u -g -F 999 "$TEST_BIN" "$@"

echo "[perf] Generating flamegraph: ${FLAME_GRAPH_SVG}"

# Generate flamegraph
perf script -i "$PERF_DATA" \
  | "$STACKCOLLAPSE" \
  | "$FLAMEGRAPH" \
  > "$FLAME_GRAPH_SVG"

# Cleanup
rm "$PERF_DATA".*

sleep 2

echo "[perf] Done: ${FLAME_GRAPH_SVG}"
