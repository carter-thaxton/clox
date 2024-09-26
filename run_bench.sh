#!/bin/bash

if [[ -z "$1" ]]; then
    echo "Usage: $0 <file.lox> [suffix]"
    echo
    echo "    - runs for at most 12 seconds, and puts results in ./bench_results directory"
    echo "    - suffix may be set, to create a unique bench result"
    exit 2
fi

if [[ -n "$2" ]]; then
  suffix=".$2"
fi
output="./bench_results/$(basename "$1")$suffix.trace"

echo "Saving results to: $output"
xctrace record --output "$output" --template "Time Profiler" --time-limit 12s --launch -- bin/clox "$1"
