#!/bin/bash
# Capture game output to see what's happening with microbe lifecycle

cd /home/dgpt/code/games/micro-idle

echo "=== Running game with diagnostics ==="
timeout 10 ./build/game.exe 2>&1 | tee game_diagnostic.log

echo ""
echo "=== Analyzing output ==="
grep -E "(Spawned|Destroyed|entity|microbe|count)" game_diagnostic.log | head -50
