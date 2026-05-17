#include "routes.hpp"
#include "utils/system_info.hpp"

#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

using json = nlohmann::json;

namespace benchforge {

Routes::Routes(const Config&  config,
               Crud&          crud,
               Scanner&       scanner,
               Runner&        runner,
               Exporter&      exporter)
    : config_(config)
    , crud_(crud)
    , scanner_(scanner)
    , runner_(runner)
    , exporter_(exporter)
{}

// ── Mount ─────────────────────────────────────────────────────────────────────

void Routes::mount(httplib::Server& http) {

    // CORS preflight
    http.Options(".*", [this](const httplib::Request&,
                               httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });

    // ── Models ────────────────────────────────────────────────────────────────
    http.Get("/api/models", [this](const httplib::Request& req,
                                    httplib::Response& res) {
        handle_get_models(req, res);
    });

    http.Post("/api/models/scan", [this](const httplib::Request& req,
                                          httplib::Response& res) {
        handle_scan_models(req, res);
    });

    http.Post("/api/models/register", [this](const httplib::Request& req,
                                              httplib::Response& res) {
        handle_register_model(req, res);
    });

    http.Delete("/api/models/(\\d+)", [this](const httplib::Request& req,
                                              httplib::Response& res) {
        handle_delete_model(req, res);
    });

    // ── Configs ───────────────────────────────────────────────────────────────
    http.Get("/api/configs", [this](const httplib::Request& req,
                                     httplib::Response& res) {
        handle_get_configs(req, res);
    });

    // ── Benchmark ─────────────────────────────────────────────────────────────
    http.Post("/api/benchmark/run", [this](const httplib::Request& req,
                                            httplib::Response& res) {
        handle_run_benchmark(req, res);
    });

    http.Get("/api/benchmark/status", [this](const httplib::Request& req,
                                              httplib::Response& res) {
        handle_benchmark_status(req, res);
    });

    http.Post("/api/benchmark/abort", [this](const httplib::Request& req,
                                              httplib::Response& res) {
        handle_abort_benchmark(req, res);
    });

    http.Get("/api/benchmark/stream", [this](const httplib::Request& req,
                                              httplib::Response& res) {
        handle_benchmark_stream(req, res);
    });

    // ── Runs ──────────────────────────────────────────────────────────────────
    http.Get("/api/runs", [this](const httplib::Request& req,
                                  httplib::Response& res) {
        handle_get_runs(req, res);
    });

    http.Get("/api/runs/(\\d+)", [this](const httplib::Request& req,
                                         httplib::Response& res) {
        handle_get_run(req, res);
    });

    http.Delete("/api/runs/(\\d+)", [this](const httplib::Request& req,
                                            httplib::Response& res) {
        handle_delete_run(req, res);
    });

    // ── Export ────────────────────────────────────────────────────────────────
    http.Post("/api/export/json", [this](const httplib::Request& req,
                                          httplib::Response& res) {
        handle_export_json(req, res);
    });

    http.Post("/api/export/csv", [this](const httplib::Request& req,
                                         httplib::Response& res) {
        handle_export_csv(req, res);
    });

    // ── System ────────────────────────────────────────────────────────────────
    http.Get("/api/system", [this](const httplib::Request& req,
                                    httplib::Response& res) {
        handle_system_info(req, res);
    });
}

// ── Models ────────────────────────────────────────────────────────────────────

void Routes::handle_get_models(const httplib::Request&,
                                httplib::Response& res) {
    try {
        auto models = crud_.get_all_models();
        json arr    = json::array();
        for (const auto& m : models) {
            arr.push_back(json::parse(model_to_json(m)));
        }
        json_response(res, 200, arr.dump());
    } catch (const std::exception& e) {
        json_response(res, 500, error_json(e.what()));
    }
}

void Routes::handle_scan_models(const httplib::Request&,
                                 httplib::Response& res) {
    try {
        auto result = scanner_.scan();
        json obj;
        obj["new_models_found"]    = result.new_models_found;
        obj["models_deactivated"]  = result.models_deactivated;
        obj["total_active_models"] = result.total_active_models;
        obj["errors"]              = result.errors;
        json_response(res, 200, obj.dump());
    } catch (const std::exception& e) {
        json_response(res, 500, error_json(e.what()));
    }
}

void Routes::handle_register_model(const httplib::Request& req,
                                    httplib::Response& res) {
    try {
        auto body = json::parse(req.body);
        if (!body.contains("path") || !body["path"].is_string()) {
            json_response(res, 400, error_json("Missing required field: path"));
            return;
        }
        std::string path = body["path"].get<std::string>();
        int model_id     = scanner_.register_model(path);
        auto model_opt   = crud_.get_model_by_id(model_id);
        if (!model_opt.has_value()) {
            json_response(res, 500, error_json("Model registered but not found"));
            return;
        }
        json_response(res, 200, model_to_json(*model_opt));
    } catch (const std::exception& e) {
        json_response(res, 400, error_json(e.what()));
    }
}

void Routes::handle_delete_model(const httplib::Request& req,
                                  httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        scanner_.unregister_model(id);
        json_response(res, 200, ok_json());
    } catch (const std::exception& e) {
        json_response(res, 400, error_json(e.what()));
    }
}

// ── Configs ───────────────────────────────────────────────────────────────────

void Routes::handle_get_configs(const httplib::Request&,
                                 httplib::Response& res) {
    try {
        json obj;

        // Presets
        json presets     = json::object();
        json quick_obj;
        quick_obj["prompt_tokens"]    = config_.preset_quick.prompt_tokens;
        quick_obj["generation_tokens"]= config_.preset_quick.generation_tokens;
        quick_obj["repetitions"]      = config_.preset_quick.repetitions;
        presets["quick"]              = quick_obj;

        json thorough_obj;
        thorough_obj["prompt_tokens"]    = config_.preset_thorough.prompt_tokens;
        thorough_obj["generation_tokens"]= config_.preset_thorough.generation_tokens;
        thorough_obj["repetitions"]      = config_.preset_thorough.repetitions;
        presets["thorough"]              = thorough_obj;
        obj["presets"]                   = presets;

        // Prompt types
        json prompt_arr = json::array();
        for (const auto& p : config_.prompt_presets) {
            json pobj;
            pobj["id"]               = p.id;
            pobj["label"]            = p.label;
            pobj["description"]      = p.description;
            pobj["prompt_tokens"]    = p.prompt_tokens;
            pobj["generation_tokens"]= p.generation_tokens;
            prompt_arr.push_back(pobj);
        }
        obj["prompt_types"]  = prompt_arr;
        obj["default_preset"]= config_.default_preset;
        obj["threads"]       = config_.threads;
        obj["gpu_layers"]    = config_.gpu_layers;

        json_response(res, 200, obj.dump());
    } catch (const std::exception& e) {
        json_response(res, 500, error_json(e.what()));
    }
}

// ── Benchmark ─────────────────────────────────────────────────────────────────

void Routes::handle_run_benchmark(const httplib::Request& req,
                                   httplib::Response& res) {
    try {
        if (runner_.is_running()) {
            json_response(res, 409, error_json("A benchmark session is already running"));
            return;
        }

        auto body = json::parse(req.body);

        // Parse model ids
        if (!body.contains("model_ids") || !body["model_ids"].is_array()) {
            json_response(res, 400, error_json("Missing required field: model_ids"));
            return;
        }
        std::vector<int> model_ids;
        for (const auto& id : body["model_ids"])
            model_ids.push_back(id.get<int>());

        if (model_ids.empty()) {
            json_response(res, 400, error_json("model_ids cannot be empty"));
            return;
        }

        // Build BenchmarkConfig from request body
        BenchmarkConfig cfg{};
        cfg.preset_name  = body.value("preset_name",  "custom");
        cfg.prompt_type  = body.value("prompt_type",  "custom");
        cfg.threads      = body.value("threads",      config_.threads);
        cfg.gpu_layers   = body.value("gpu_layers",   config_.gpu_layers);
        cfg.repetitions  = body.value("repetitions",  config_.repetitions);
        cfg.include_perplexity = body.value("include_perplexity", false);

        // Resolve token counts from preset or request body
        if (cfg.preset_name == "quick") {
            cfg.prompt_tokens    = config_.preset_quick.prompt_tokens;
            cfg.generation_tokens= config_.preset_quick.generation_tokens;
            cfg.repetitions      = config_.preset_quick.repetitions;
        } else if (cfg.preset_name == "thorough") {
            cfg.prompt_tokens    = config_.preset_thorough.prompt_tokens;
            cfg.generation_tokens= config_.preset_thorough.generation_tokens;
            cfg.repetitions      = config_.preset_thorough.repetitions;
        } else {
            cfg.prompt_tokens    = body.value("prompt_tokens",    128);
            cfg.generation_tokens= body.value("generation_tokens", 128);
        }

        // Override with prompt type token counts if specified
        for (const auto& p : config_.prompt_presets) {
            if (p.id == cfg.prompt_type) {
                cfg.prompt_tokens     = p.prompt_tokens;
                cfg.generation_tokens = p.generation_tokens;
                break;
            }
        }

        // Persist config
        cfg.id = crud_.insert_config(cfg);

        // Build request
        BenchmarkRequest request;
        request.model_ids = model_ids;
        request.config_id = cfg.id;
        request.config    = cfg;

        // Run on background thread
        std::thread([this, request]() {
            runner_.run(request, [this](const ProgressEvent& event) {
                broadcast_sse(event);
            });
        }).detach();

        json obj;
        obj["message"]   = "Benchmark session started";
        obj["model_count"]= (int)model_ids.size();
        obj["config_id"] = cfg.id;
        json_response(res, 200, obj.dump());

    } catch (const std::exception& e) {
        json_response(res, 400, error_json(e.what()));
    }
}

void Routes::handle_benchmark_status(const httplib::Request&,
                                      httplib::Response& res) {
    json obj;
    obj["running"] = runner_.is_running();
    json_response(res, 200, obj.dump());
}

void Routes::handle_abort_benchmark(const httplib::Request&,
                                     httplib::Response& res) {
    runner_.abort();
    json obj;
    obj["message"] = "Abort requested. Current model will finish.";
    json_response(res, 200, obj.dump());
}

void Routes::handle_benchmark_stream(const httplib::Request&,
                                      httplib::Response& res) {
    res.set_header("Content-Type",  "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection",    "keep-alive");
    res.set_header("Access-Control-Allow-Origin", "*");

    res.set_chunked_content_provider("text/event-stream",
        [this](size_t, httplib::DataSink& sink) {
            // Register this sink
            {
                std::lock_guard<std::mutex> lock(sse_mutex_);
                sse_sinks_.push_back(&sink);
            }

            // Send initial heartbeat
            std::string heartbeat = "data: {\"type\":\"heartbeat\"}\n\n";
            sink.write(heartbeat.c_str(), heartbeat.size());

            // Keep connection alive until client disconnects
            while (sink.is_writable()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Unregister sink on disconnect
            std::lock_guard<std::mutex> lock(sse_mutex_);
            sse_sinks_.erase(
                std::remove(sse_sinks_.begin(), sse_sinks_.end(), &sink),
                sse_sinks_.end()
            );
            return true;
        }
    );
}

// ── Runs ──────────────────────────────────────────────────────────────────────

void Routes::handle_get_runs(const httplib::Request&,
                              httplib::Response& res) {
    try {
        auto summaries = crud_.get_all_run_summaries();
        json arr       = json::array();
        for (const auto& s : summaries)
            arr.push_back(json::parse(summary_to_json(s)));
        json_response(res, 200, arr.dump());
    } catch (const std::exception& e) {
        json_response(res, 500, error_json(e.what()));
    }
}

void Routes::handle_get_run(const httplib::Request& req,
                             httplib::Response& res) {
    try {
        int  id      = std::stoi(req.matches[1]);
        auto runs    = crud_.get_run_summaries_by_ids({id});
        if (runs.empty()) {
            json_response(res, 404, error_json("Run not found"));
            return;
        }
        json_response(res, 200, summary_to_json(runs[0]));
    } catch (const std::exception& e) {
        json_response(res, 400, error_json(e.what()));
    }
}

void Routes::handle_delete_run(const httplib::Request& req,
                                httplib::Response& res) {
    try {
        int id = std::stoi(req.matches[1]);
        crud_.delete_run(id);
        json_response(res, 200, ok_json());
    } catch (const std::exception& e) {
        json_response(res, 400, error_json(e.what()));
    }
}

// ── Export ────────────────────────────────────────────────────────────────────

void Routes::handle_export_json(const httplib::Request& req,
                                 httplib::Response& res) {
    try {
        auto body    = json::parse(req.body);
        auto run_ids = body["run_ids"].get<std::vector<int>>();
        auto runs    = crud_.get_run_summaries_by_ids(run_ids);
        auto result  = exporter_.exportRuns(runs, ExportFormat::JSON);

        if (!result.success) {
            json_response(res, 500, error_json(result.error));
            return;
        }

        json obj;
        obj["file_path"] = result.file_path;
        obj["message"]   = "Exported " + std::to_string(runs.size()) + " runs";
        json_response(res, 200, obj.dump());
    } catch (const std::exception& e) {
        json_response(res, 400, error_json(e.what()));
    }
}

void Routes::handle_export_csv(const httplib::Request& req,
                                httplib::Response& res) {
    try {
        auto body    = json::parse(req.body);
        auto run_ids = body["run_ids"].get<std::vector<int>>();
        auto runs    = crud_.get_run_summaries_by_ids(run_ids);
        auto result  = exporter_.exportRuns(runs, ExportFormat::CSV);

        if (!result.success) {
            json_response(res, 500, error_json(result.error));
            return;
        }

        json obj;
        obj["file_path"] = result.file_path;
        obj["message"]   = "Exported " + std::to_string(runs.size()) + " runs";
        json_response(res, 200, obj.dump());
    } catch (const std::exception& e) {
        json_response(res, 400, error_json(e.what()));
    }
}

// ── System Info ───────────────────────────────────────────────────────────────

void Routes::handle_system_info(const httplib::Request&,
                                 httplib::Response& res) {
    try {
        auto snap = SystemInfo::snapshot();
        json obj;
        obj["cpu_name"]      = SystemInfo::cpu_name();
        obj["cpu_cores"]     = SystemInfo::cpu_core_count();
        obj["gpu_name"]      = SystemInfo::gpu_name();
        obj["gpu_available"] = snap.gpu_available;
        obj["ram_total_mb"]  = snap.ram_total_mb;
        obj["ram_used_mb"]   = snap.ram_used_mb;

        if (snap.gpu_available) {
            obj["vram_total_mb"] = snap.vram_total_mb;
            obj["vram_used_mb"]  = snap.vram_used_mb;
        } else {
            obj["vram_total_mb"] = nullptr;
            obj["vram_used_mb"]  = nullptr;
        }

        json_response(res, 200, obj.dump());
    } catch (const std::exception& e) {
        json_response(res, 500, error_json(e.what()));
    }
}

// ── SSE Broadcaster ───────────────────────────────────────────────────────────

void Routes::broadcast_sse(const ProgressEvent& event) {
    json obj;
    obj["type"]       = event.type;
    obj["run_id"]     = event.run_id;
    obj["model_id"]   = event.model_id;
    obj["model_name"] = event.model_name;
    obj["current"]    = event.current;
    obj["total"]      = event.total;
    obj["message"]    = event.message;
    obj["progress"]   = event.progress;

    std::string data = "data: " + obj.dump() + "\n\n";

    std::lock_guard<std::mutex> lock(sse_mutex_);
    for (auto* sink : sse_sinks_) {
        if (sink && sink->is_writable()) {
            sink->write(data.c_str(), data.size());
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void Routes::json_response(httplib::Response& res,
                            int               status,
                            const std::string& body) const {
    res.status = status;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content(body, "application/json");
}

std::string Routes::error_json(const std::string& message) {
    json obj;
    obj["error"] = message;
    return obj.dump();
}

std::string Routes::ok_json(const std::string& data) {
    json obj;
    obj["ok"]   = true;
    obj["data"] = json::parse(data);
    return obj.dump();
}

std::string Routes::model_to_json(const Model& m) {
    json obj;
    obj["id"]         = m.id;
    obj["name"]       = m.name;
    obj["path"]       = m.path;
    obj["size_label"] = m.size_label;
    obj["file_size"]  = m.file_size;
    obj["added_at"]   = m.added_at;
    obj["is_active"]  = m.is_active;
    return obj.dump();
}

std::string Routes::summary_to_json(const RunSummary& s) {
    json obj;
    obj["run_id"]                 = s.run_id;
    obj["model_name"]             = s.model_name;
    obj["model_path"]             = s.model_path;
    obj["prompt_type"]            = s.prompt_type;
    obj["preset_name"]            = s.preset_name;
    obj["status"]                 = s.status;
    obj["started_at"]             = s.started_at;
    obj["tokens_per_second"]      = s.tokens_per_second;
    obj["prompt_tokens_per_sec"]  = s.prompt_tokens_per_sec;
    obj["gen_tokens_per_sec"]     = s.gen_tokens_per_sec;
    obj["time_to_first_token_ms"] = s.time_to_first_token_ms;
    obj["ram_usage_mb"]           = s.ram_usage_mb;
    obj["vram_usage_mb"]          = s.vram_usage_mb >= 0.0 ? json(s.vram_usage_mb) : json(nullptr);
    obj["perplexity"]             = s.perplexity   >= 0.0 ? json(s.perplexity)     : json(nullptr);
    return obj.dump();
}

} // namespace benchforge