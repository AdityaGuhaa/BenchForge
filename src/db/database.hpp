#pragma once

#include <string>
#include <SQLiteCpp/SQLiteCpp.h>

namespace benchforge {

// ── Database ──────────────────────────────────────────────────────────────────
// Owns the SQLite connection and handles schema initialization.
// One instance lives for the lifetime of the application.
class Database {
public:
    // Opens (or creates) the SQLite DB at the given path.
    // Creates all tables if they don't exist.
    // Throws std::runtime_error on failure.
    explicit Database(const std::string& db_path);

    // Non-copyable, non-movable -- one connection per app.
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // Returns a reference to the underlying SQLiteCpp connection.
    // Used by crud.cpp to execute queries.
    SQLite::Database& connection();

private:
    SQLite::Database db_;

    void create_tables();
    void create_models_table();
    void create_configs_table();
    void create_runs_table();
    void create_results_table();
};

} // namespace benchforge