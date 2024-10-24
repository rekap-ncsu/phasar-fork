must be on commit `956972ae2533efdec284ef07576fc01054a71167` (v0323) for example taint analysis to work. v2403 broke taint analysis somehow as of Oct 11, 2024

generate main.ll:
```
cargo rustc -- --emit=llvm-ir
```

find the ll file in `target/debug/deps/*.ll`, and find the mangled source and sink names

run analysis with entry point specified:
```
phasar-cli -m target/debug/deps/simple_str_rs-233e9c805b7721fc.ll -D ifds-taint --analysis-config=analysis-config.json --entry-points="_ZN13simple_str_rs4main17h0ce9277909836a48E"
```