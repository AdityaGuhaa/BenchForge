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
- `config.yaml` — runtime configuration

## Build

```
cmake -S . -B build
cmake --build build
```

## License

Licensed under the [Apache License 2.0](LICENSE).
