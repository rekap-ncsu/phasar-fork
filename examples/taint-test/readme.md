generate main.ll:
`/usr/local/llvm-14/bin/clang++ -emit-llvm -S -fno-discard-value-names  main.cpp`

must be on commit `956972ae2533efdec284ef07576fc01054a71167` (v0323) for example taint analysis to work. v2403 broke taint analysis somehow as of Oct 11, 2024

run analysis:
`phasar-cli -m main.ll -D ifds-taint --analysis-config=analysis-config.json`