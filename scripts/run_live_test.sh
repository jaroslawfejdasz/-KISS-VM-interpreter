#!/bin/bash
# run_live_test.sh — Start a Minima testnet node and run live integration tests
# Usage: ./scripts/run_live_test.sh [minima.jar path]

set -e

MINIMA_JAR="${1:-/tmp/minima-node/minima.jar}"
DATA_DIR="/tmp/minima-live-test"
LOG="/tmp/minima-live.log"
BUILD_DIR="${BUILD_DIR:-/tmp/build}"

echo "=== Minima Live Test Runner ==="
echo "JAR: $MINIMA_JAR"
echo "Data: $DATA_DIR"
echo ""

# Check binary exists
if [ ! -f "$MINIMA_JAR" ]; then
    echo "ERROR: minima.jar not found at $MINIMA_JAR"
    echo "Download from: https://github.com/minima-global/Minima/releases"
    exit 1
fi

# Clean start
pkill -f "java.*minima" 2>/dev/null; sleep 1
rm -rf "$DATA_DIR" && mkdir -p "$DATA_DIR"

# Start Minima node (genesis + test mode + noshutdownhook)
echo "Starting Minima node..."
java -Xmx256m -jar "$MINIMA_JAR" \
  -data "$DATA_DIR" \
  -port 9001 \
  -rpcenable \
  -test \
  -genesis \
  -noshutdownhook \
  > "$LOG" 2>&1 &

JPID=$!
echo "PID: $JPID"

# Wait for genesis block
echo "Waiting for genesis block..."
for i in $(seq 1 30); do
    sleep 1
    if grep -q "Genesis block created" "$LOG" 2>/dev/null; then
        echo "Genesis block created at t=${i}s"
        break
    fi
    [ $((i % 5)) -eq 0 ] && echo "  t=${i}s..."
done

# Wait a bit more for networking
sleep 3

# Verify RPC responds
RPC_STATUS=$(curl -s --max-time 3 "http://127.0.0.1:9005/status" 2>/dev/null)
if [ -z "$RPC_STATUS" ]; then
    echo "ERROR: RPC not responding"
    cat "$LOG"
    kill $JPID 2>/dev/null
    exit 1
fi

BLOCK=$(echo "$RPC_STATUS" | python3 -c "import sys,json; print(json.load(sys.stdin)['response']['chain']['block'])" 2>/dev/null)
echo "Node ready. Current block: $BLOCK"
echo ""

# Build test if needed
if [ ! -f "$BUILD_DIR/tests/test_live_node" ]; then
    echo "Building test_live_node..."
    make -C "$BUILD_DIR" tests/test_live_node -j$(nproc)
fi

# Run tests
echo "=== Running live integration tests ==="
"$BUILD_DIR/tests/test_live_node"
TEST_EXIT=$?

# Cleanup
echo ""
echo "Stopping Minima node..."
kill $JPID 2>/dev/null
wait $JPID 2>/dev/null

exit $TEST_EXIT
