#pragma once

#include "database.hpp"
#include "models.hpp"
#include <vector>
#include <optional>

namespace benchforge {

class Crud {
public:
    explicit Crud(Database& db);

    // ── Models ────────────────────────────────────────────────────────────────
    // Insert a new model. Returns the new row id.
    int  insert_model(const Model& model);

    // Returns all active models.
    std::vector<Model> get_all_models();

    // Returns a single model by id. Empty optional if not found.
    std::optional<Model> get_model_by_id(int id);

    // Returns a model by its file path. Empty optional if not found.
    std::optional<Model> get_model_by_path(const std::string& path);

    // Marks a model as inactive (file moved/deleted). Does not delete the row
    // so historical run data is preserved.
    void deactivate_model(int id);

    // ── BenchmarkConfig ───────────────────────────────────────────────────────
    // Insert a benchmark config. Returns the new row id.
    int insert_config(const BenchmarkConfig& config);

    // ── BenchmarkRun ──────────────────────────────────────────────────────────
    // Insert a new run record with status "pending". Returns the new row id.
    int  insert_run(int model_id, int config_id);

    // Update run status ("running", "done", "failed").
    void update_run_status(int run_id, const std::string& status,
                           const std::string& error_message = "");

    // Set started_at to current UTC time.
    void mark_run_started(int run_id);

    // Set finished_at to current UTC time.
    void mark_run_finished(int run_id);

    // ── BenchmarkResult ───────────────────────────────────────────────────────
    // Insert measured metrics for a completed run.
    void insert_result(const BenchmarkResult& result);

    // ── Queries ───────────────────────────────────────────────────────────────
    // Returns all RunSummary rows (joined view), newest first.
    std::vector<RunSummary> get_all_run_summaries();

    // Returns RunSummary rows filtered by a list of run ids.
    std::vector<RunSummary> get_run_summaries_by_ids(const std::vector<int>& run_ids);

    // Returns all runs for a specific model.
    std::vector<RunSummary> get_runs_for_model(int model_id);

    // Deletes a run and its associated result. Model row is preserved.
    void delete_run(int run_id);

private:
    SQLite::Database& db_;

    // Helper: map a query row to a RunSummary struct.
    RunSummary row_to_summary(SQLite::Statement& stmt);

    // Helper: map a query row to a Model struct.
    Model row_to_model(SQLite::Statement& stmt);
};

} // namespace benchforge