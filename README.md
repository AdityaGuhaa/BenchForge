# BenchForge

A benchmarking workbench for local LLMs, with a C++ core and a lightweight web frontend.

## Project layout

- `src/` — C++ sources
  - `benchmark/` — runner, metrics, perplexity
  - `config/` — configuration loading
  - `db/` — storage layer (models, CRUD, database)
  - `discovery/` — model/file scanning
  - `export/` — result export
  - `server/` — HTTP server and routes
  - `utils/` — system info and helpers
- `frontend/` — static UI (HTML/CSS/JS)
- `docs/` — architecture and contribution notes
- `config.toml` — runtime configuration
- `third_party/` — vendored dependencies (cpp-httplib, nlohmann/json, tomlplusplus, SQLiteCpp) as git submodules

## Build

Clone with submodules:

```
git clone --recurse-submodules https://github.com/AdityaGuhaa/BenchForge.git
```

Or, if already cloned:

```
git submodule update --init --recursive
```

Configure and build:

```
cmake -S . -B build
cmake --build build --config Release
```

The binary lands in `build/bin/`. Run `BenchForge.exe` from there and open http://localhost:7860.

## License

Licensed under the [Apache License 2.0](LICENSE).
