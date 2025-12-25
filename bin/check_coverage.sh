#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:-build}
threshold=${COVERAGE_THRESHOLD:-95}

sources=(
  engine/platform/engine.c
  engine/platform/time.c
  engine/util/rng.c
  game/game.c
  game/gpu_sim.c
)

sum_total=0
sum_executed=0

for src in "${sources[@]}"; do
  obj_dir="$build_dir/CMakeFiles/tests.dir/$(dirname "$src")"
  if [[ ! -d "$obj_dir" ]]; then
    echo "missing object dir: $obj_dir" >&2
    exit 1
  fi
  obj_file="$(basename "$src").o"
  out=$(gcov -b -c -o "$obj_dir" "$obj_dir/$obj_file" | tr -d '\r')
  line=$(echo "$out" | rg -m1 "Lines executed:" || true)
  if [[ -z "$line" ]]; then
    echo "missing coverage line for $src" >&2
    exit 1
  fi
  percent=$(echo "$line" | sed -E 's/Lines executed:([0-9.]+)% of ([0-9]+).*/\1/')
  total=$(echo "$line" | sed -E 's/Lines executed:([0-9.]+)% of ([0-9]+).*/\2/')
  executed=$(awk -v p="$percent" -v t="$total" 'BEGIN { printf "%.0f", (p/100.0)*t }')
  sum_total=$((sum_total + total))
  sum_executed=$((sum_executed + executed))

done

if [[ $sum_total -eq 0 ]]; then
  echo "no coverage data" >&2
  exit 1
fi

coverage=$(awk -v e="$sum_executed" -v t="$sum_total" 'BEGIN { printf "%.2f", (e/t)*100.0 }')

echo "Total line coverage: ${coverage}%"

awk -v c="$coverage" -v t="$threshold" 'BEGIN { exit (c+0 < t) }'
