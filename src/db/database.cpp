#include "database.hpp"
#include <stdexcept>
#include <filesystem>

namespace benchforge {

Database::Database(const std::string& db_path)
    : db_(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
{
    // Ensure parent directory exists
    std::filesystem::path p(db_path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    // Enable WAL mode for better concurrent read performance
    db_.exec("PRAGMA journal_mode=WAL;");

    // Enable foreign key enforcement
    db_.exec("PRAGMA foreign_keys=ON;");

    create_tables();
}

SQLite::Database& Database::connection() {
    return db_;
}

void Database::create_tables() {
    create_models_table();
    create_configs_table();
    create_runs_table();
    create_results_table();
}

void Database::create_models_table() {
    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS models (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT    NOT NULL,
            path        TEXT    NOT NULL UNIQUE,
            size_label  TEXT    NOT NULL DEFAULT '',
            file_size   INTEGER NOT NULL DEFAULT 0,
            added_at    TEXT    NOT NULL DEFAULT (datetime('now')),
            is_active   INTEGER NOT NULL DEFAULT 1
        );
    )");
}

void Database::create_configs_table() {
    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS benchmark_configs (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            preset_name         TEXT    NOT NULL DEFAULT 'custom',
            prompt_type         TEXT    NOT NULL DEFAULT 'custom',
            prompt_tokens       INTEGER NOT NULL,
            generation_tokens   INTEGER NOT NULL,
            repetitions         INTEGER NOT NULL DEFAULT 1,
            threads             INTEGER NOT NULL DEFAULT 8,
            gpu_layers          INTEGER NOT NULL DEFAULT 99,
            include_perplexity  INTEGER NOT NULL DEFAULT 0
        );
    )");
}

void Database::create_runs_table() {
    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS benchmark_runs (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            model_id        INTEGER NOT NULL REFERENCES models(id),
            config_id       INTEGER NOT NULL REFERENCES benchmark_configs(id),
            status          TEXT    NOT NULL DEFAULT 'pending',
            started_at      TEXT,
            finished_at     TEXT,
            error_message   TEXT    NOT NULL DEFAULT ''
        );
    )");
}

void Database::create_results_table() {
    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS benchmark_results (
            id                      INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id                  INTEGER NOT NULL REFERENCES benchmark_runs(id),
            tokens_per_second       REAL    NOT NULL DEFAULT 0.0,
            prompt_tokens_per_sec   REAL    NOT NULL DEFAULT 0.0,
            gen_tokens_per_sec      REAL    NOT NULL DEFAULT 0.0,
            time_to_first_token_ms  REAL    NOT NULL DEFAULT 0.0,
            ram_usage_mb            REAL    NOT NULL DEFAULT 0.0,
            vram_usage_mb           REAL    NOT NULL DEFAULT -1.0,
            perplexity              REAL    NOT NULL DEFAULT -1.0,
            raw_output              TEXT    NOT NULL DEFAULT ''
        );
    )");
}

} // namespace benchforge