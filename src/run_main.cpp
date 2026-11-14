#include "build_info.hpp"
#include "export.hpp"
#include "input.hpp"
#include "replay.hpp"
#include "scenario.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace ravel;

namespace {
void write_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out || !(out << text)) throw std::runtime_error("cannot write " + path.string());
}
}

// ravel-run <scenario.json> <output_dir> <git_commit>
// Paths inside the scenario are relative to the current directory (run from the repository root).
// Writes <output_dir>/<scenario>/<policy>/{trace.csv, deliveries.csv, summary.json, manifest.json}.
int main(int argc, char** argv) try {
    if (argc != 4) throw std::invalid_argument("usage: ravel-run <scenario.json> <output_dir> <git_commit>");
    const std::string scenario_file = argv[1];
    const std::string scenario_text = read_file(scenario_file);
    const Scenario sc = parse_scenario(scenario_text);
    const std::string input = read_file(sc.input_path);
    if (fnv1a64(input) != sc.input_fnv1a64)
        throw std::runtime_error("input " + sc.input_path + " has fnv1a64 " + hex64(fnv1a64(input)) +
                                 ", scenario expects " + hex64(sc.input_fnv1a64));
    const std::vector<Event> tape = parse_commands(input);
    Scenario control_sc = sc;
    control_sc.faults.clear();
    const BuildInfo build{argv[3], build::compiler, build::build_type, build::flags};

    for (Policy policy : sc.policies) {
        const RunResult run = run_replay(tape, sc, policy);
        const RunResult control = run_replay(tape, control_sc, policy);
        const std::filesystem::path dir = std::filesystem::path(argv[2]) / sc.name / to_string(policy);
        std::filesystem::create_directories(dir);
        const std::string deliveries = deliveries_csv(run.deliveries);
        write_file(dir / "trace.csv", trace_csv(run.trace));
        write_file(dir / "deliveries.csv", deliveries);
        write_file(dir / "summary.json", summary_json(sc, run.summary, control.summary));
        write_file(dir / "manifest.json", manifest_json(build, scenario_file, fnv1a64(scenario_text),
                                                        sc.input_fnv1a64, fnv1a64(deliveries)));
        std::cout << dir.string() << '\n';
    }
} catch (const std::exception& e) {std::cerr << "ravel-run: " << e.what() << '\n'; return 1;}
