#include "crud.hpp"
#include <stdexcept>

namespace benchforge {

Crud::Crud(Database& db)
    : db_(db.connection())
{}

// ── Models ────────────────────────────────────────────────────────────────────

int Crud::insert_model(const Model& model) {
    SQLite::Statement stmt(db_, R"(
        INSERT OR IGNORE INTO models (name, path, size_label, file_size, is_active)
        VALUES (?, ?, ?, ?, 1)
    )");
    stmt.bind(1, model.name);
    stmt.bind(2, model.path);
    stmt.bind(3, model.size_label);
    stmt.bind(4, (int64_t)model.file_size);
    stmt.exec();
    return (int)db_.getLastInsertRowid();
}

std::vector<Model> Crud::get_all_models() {
    SQLite::Statement stmt(db_, R"(
        SELECT id, name, path, size_label, file_size, added_at, is_active
        FROM models
        WHERE is_active = 1
        ORDER BY name ASC
    )");
    std::vector<Model> models;
    while (stmt.executeStep())
        models.push_back(row_to_model(stmt));
    return models;
}

std::optional<Model> Crud::get_model_by_id(int id) {
    SQLite::Statement stmt(db_, R"(
        SELECT id, name, path, size_label, file_size, added_at, is_active
        FROM models WHERE id = ?
    )");
    stmt.bind(1, id);
    if (stmt.executeStep())
        return row_to_model(stmt);
    return std::nullopt;
}

std::optional<Model> Crud::get_model_by_path(const std::string& path) {
    SQLite::Statement stmt(db_, R"(
        SELECT id, name, path, size_label, file_size, added_at, is_active
        FROM models WHERE path = ?
    )");
    stmt.bind(1, path);
    if (stmt.executeStep())
        return row_to_model(stmt);
    return std::nullopt;
}

void Crud::deactivate_model(int id) {
    SQLite::Statement stmt(db_, "UPDATE models SET is_active = 0 WHERE id = ?");
    stmt.bind(1, id);
    stmt.exec();
}

// ── BenchmarkConfig ───────────────────────────────────────────────────────────

int Crud::insert_config(const BenchmarkConfig& cfg) {
    SQLite::Statement stmt(db_, R"(
        INSERT INTO benchmark_configs
            (preset_name, prompt_type, prompt_tokens, generation_tokens,
             repetitions, threads, gpu_layers, include_perplexity)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind(1, cfg.preset_name);
    stmt.bind(2, cfg.prompt_type);
    stmt.bind(3, cfg.prompt_tokens);
    stmt.bind(4, cfg.generation_tokens);
    stmt.bind(5, cfg.repetitions);
    stmt.bind(6, cfg.threads);
    stmt.bind(7, cfg.gpu_layers);
    stmt.bind(8, (int)cfg.include_perplexity);
    stmt.exec();
    return (int)db_.getLastInsertRowid();
}

// ── BenchmarkRun ──────────────────────────────────────────────────────────────

int Crud::insert_run(int model_id, int config_id) {
    SQLite::Statement stmt(db_, R"(
        INSERT INTO benchmark_runs (model_id, config_id, status)
        VALUES (?, ?, 'pending')
    )");
    stmt.bind(1, model_id);
    stmt.bind(2, config_id);
    stmt.exec();
    return (int)db_.getLastInsertRowid();
}

void Crud::update_run_status(int run_id, const std::string& status,
                              const std::string& error_message) {
    SQLite::Statement stmt(db_, R"(
        UPDATE benchmark_runs
        SET status = ?, error_message = ?
        WHERE id = ?
    )");
    stmt.bind(1, status);
    stmt.bind(2, error_message);
    stmt.bind(3, run_id);
    stmt.exec();
}

void Crud::mark_run_started(int run_id) {
    SQLite::Statement stmt(db_, R"(
        UPDATE benchmark_runs
        SET started_at = datetime('now'), status = 'running'
        WHERE id = ?
    )");
    stmt.bind(1, run_id);
    stmt.exec();
}

void Crud::mark_run_finished(int run_id) {
    SQLite::Statement stmt(db_, R"(
        UPDATE benchmark_runs
        SET finished_at = datetime('now')
        WHERE id = ?
    )");
    stmt.bind(1, run_id);
    stmt.exec();
}

// ── BenchmarkResult ───────────────────────────────────────────────────────────

void Crud::insert_result(const BenchmarkResult& r) {
    SQLite::Statement stmt(db_, R"(
        INSERT INTO benchmark_results
            (run_id, tokens_per_second, prompt_tokens_per_sec, gen_tokens_per_sec,
             time_to_first_token_ms, ram_usage_mb, vram_usage_mb, perplexity, raw_output)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind(1, r.run_id);
    stmt.bind(2, r.tokens_per_second);
    stmt.bind(3, r.prompt_tokens_per_sec);
    stmt.bind(4, r.gen_tokens_per_sec);
    stmt.bind(5, r.time_to_first_token_ms);
    stmt.bind(6, r.ram_usage_mb);
    stmt.bind(7, r.vram_usage_mb);
    stmt.bind(8, r.perplexity);
    stmt.bind(9, r.raw_output);
    stmt.exec();
}

// ── Queries ───────────────────────────────────────────────────────────────────

std::vector<RunSummary> Crud::get_all_run_summaries() {
    SQLite::Statement stmt(db_, R"(
        SELECT
            r.id, m.name, m.path,
            c.prompt_type, c.preset_name,
            r.status, r.started_at,
            res.tokens_per_second, res.prompt_tokens_per_sec,
            res.gen_tokens_per_sec, res.time_to_first_token_ms,
            res.ram_usage_mb, res.vram_usage_mb, res.perplexity
        FROM benchmark_runs r
        JOIN models              m   ON r.model_id  = m.id
        JOIN benchmark_configs   c   ON r.config_id = c.id
        LEFT JOIN benchmark_results res ON res.run_id = r.id
        ORDER BY r.id DESC
    )");
    std::vector<RunSummary> summaries;
    while (stmt.executeStep())
        summaries.push_back(row_to_summary(stmt));
    return summaries;
}

std::vector<RunSummary> Crud::get_run_summaries_by_ids(const std::vector<int>& run_ids) {
    std::vector<RunSummary> summaries;
    for (int rid : run_ids) {
        SQLite::Statement stmt(db_, R"(
            SELECT
                r.id, m.name, m.path,
                c.prompt_type, c.preset_name,
                r.status, r.started_at,
                res.tokens_per_second, res.prompt_tokens_per_sec,
                res.gen_tokens_per_sec, res.time_to_first_token_ms,
                res.ram_usage_mb, res.vram_usage_mb, res.perplexity
            FROM benchmark_runs r
            JOIN models              m   ON r.model_id  = m.id
            JOIN benchmark_configs   c   ON r.config_id = c.id
            LEFT JOIN benchmark_results res ON res.run_id = r.id
            WHERE r.id = ?
        )");
        stmt.bind(1, rid);
        if (stmt.executeStep())
            summaries.push_back(row_to_summary(stmt));
    }
    return summaries;
}

std::vector<RunSummary> Crud::get_runs_for_model(int model_id) {
    SQLite::Statement stmt(db_, R"(
        SELECT
            r.id, m.name, m.path,
            c.prompt_type, c.preset_name,
            r.status, r.started_at,
            res.tokens_per_second, res.prompt_tokens_per_sec,
            res.gen_tokens_per_sec, res.time_to_first_token_ms,
            res.ram_usage_mb, res.vram_usage_mb, res.perplexity
        FROM benchmark_runs r
        JOIN models              m   ON r.model_id  = m.id
        JOIN benchmark_configs   c   ON r.config_id = c.id
        LEFT JOIN benchmark_results res ON res.run_id = r.id
        WHERE r.model_id = ?
        ORDER BY r.id DESC
    )");
    stmt.bind(1, model_id);
    std::vector<RunSummary> summaries;
    while (stmt.executeStep())
        summaries.push_back(row_to_summary(stmt));
    return summaries;
}

void Crud::delete_run(int run_id) {
    // Delete result first (FK constraint)
    SQLite::Statement del_result(db_,
        "DELETE FROM benchmark_results WHERE run_id = ?");
    del_result.bind(1, run_id);
    del_result.exec();

    SQLite::Statement del_run(db_,
        "DELETE FROM benchmark_runs WHERE id = ?");
    del_run.bind(1, run_id);
    del_run.exec();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

Model Crud::row_to_model(SQLite::Statement& stmt) {
    Model m;
    m.id         = stmt.getColumn(0).getInt();
    m.name       = stmt.getColumn(1).getText();
    m.path       = stmt.getColumn(2).getText();
    m.size_label = stmt.getColumn(3).getText();
    m.file_size  = stmt.getColumn(4).getInt64();
    m.added_at   = stmt.getColumn(5).getText();
    m.is_active  = stmt.getColumn(6).getInt() == 1;
    return m;
}

RunSummary Crud::row_to_summary(SQLite::Statement& stmt) {
    RunSummary s;
    s.run_id                 = stmt.getColumn(0).getInt();
    s.model_name             = stmt.getColumn(1).getText();
    s.model_path             = stmt.getColumn(2).getText();
    s.prompt_type            = stmt.getColumn(3).getText();
    s.preset_name            = stmt.getColumn(4).getText();
    s.status                 = stmt.getColumn(5).getText();
    s.started_at             = stmt.getColumn(6).getText();
    s.tokens_per_second      = stmt.getColumn(7).isNull() ? 0.0 : stmt.getColumn(7).getDouble();
    s.prompt_tokens_per_sec  = stmt.getColumn(8).isNull() ? 0.0 : stmt.getColumn(8).getDouble();
    s.gen_tokens_per_sec     = stmt.getColumn(9).isNull() ? 0.0 : stmt.getColumn(9).getDouble();
    s.time_to_first_token_ms = stmt.getColumn(10).isNull() ? 0.0 : stmt.getColumn(10).getDouble();
    s.ram_usage_mb           = stmt.getColumn(11).isNull() ? 0.0 : stmt.getColumn(11).getDouble();
    s.vram_usage_mb          = stmt.getColumn(12).isNull() ? -1.0 : stmt.getColumn(12).getDouble();
    s.perplexity             = stmt.getColumn(13).isNull() ? -1.0 : stmt.getColumn(13).getDouble();
    return s;
}

} // namespace benchforge