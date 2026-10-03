// A Bowtie (https://github.com/bowtie-json-schema/bowtie) harness for the corvus-json-schema C library, through its
// C++ wrapper.
//
// It speaks IHOP (one JSON request per line on standard input, one response per line on standard output):
// - `start` reports the implementation and its dialects;
// - `dialect` sets the dialect for schemas without `$schema`;
// - `run` compiles the case's schema with the case's `registry` as the document resolver and validates each instance
//   (for `annotations` output, through a verbose collector, reporting each annotation with its instance location and
//   `#…` keyword location);
// - `stop` exits.
#include <cctype>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sys/utsname.h>

#include <nlohmann/json.hpp>

#include "corvus_json_schema.hpp"

namespace cjs = corvus::json_schema;
using json = nlohmann::ordered_json;

static const std::vector<std::pair<std::string, cjs::dialect>> dialects = {
    {"https://json-schema.org/draft/2020-12/schema", cjs::dialect::draft2020_12},
    {"https://json-schema.org/draft/2019-09/schema", cjs::dialect::draft2019_09},
    {"http://json-schema.org/draft-07/schema#", cjs::dialect::draft7},
    {"http://json-schema.org/draft-06/schema#", cjs::dialect::draft6},
    {"http://json-schema.org/draft-04/schema#", cjs::dialect::draft4},
};

static json errored(const std::string& message) {
    return {{"errored", true}, {"context", {{"message", message}}}};
}

static std::string strip_fragment(const std::string& uri) {
    return uri.substr(0, uri.find('#'));
}

// Percent-encodes text as a URI fragment does (upper-case hex, UTF-8), keeping the characters a fragment allows.
static std::string percent_encode(const std::string& text) {
    static const std::string safe = "-._~!$&'()*+,;=:@/?";
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        if (c < 128 && (std::isalnum(c) || safe.find(static_cast<char>(c)) != std::string::npos)) {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

static std::string unescape_token(std::string token) {
    for (std::size_t i = token.find("~1"); i != std::string::npos; i = token.find("~1", i + 1)) {
        token.replace(i, 2, "/");
    }
    for (std::size_t i = token.find("~0"); i != std::string::npos; i = token.find("~0", i + 1)) {
        token.replace(i, 2, "~");
    }
    return token;
}

// The annotations a verbose collector grouped (instance location, then keyword as a JSON-pointer token, then schema
// location as a `#…` fragment; the same in every Corvus implementation) as Bowtie lists them: each with its keyword
// unescaped, and its keyword location the schema location's fragment followed by `/` and the keyword token,
// percent-encoded as the fragment is.
static json annotations_of(const json& grouped) {
    json found = json::array();
    for (auto& [instance_location, keywords] : grouped.items()) {
        for (auto& [token, locations] : keywords.items()) {
            for (auto& [schema_location, value] : locations.items()) {
                found.push_back({
                    {"keyword", unescape_token(token)},
                    {"instanceLocation", instance_location},
                    {"keywordLocation", schema_location + "/" + percent_encode(token)},
                    {"annotation", value},
                });
            }
        }
    }
    return found;
}

int main() {
    bool started = false;
    cjs::dialect dialect = cjs::dialect::draft2020_12;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) {
            continue;
        }
        json request = json::parse(line);
        std::string cmd = request.value("cmd", "");
        json response;
        if (cmd == "start") {
            if (request.value("version", 0) != 1) {
                throw std::runtime_error("Unsupported IHOP version " + request["version"].dump());
            }
            started = true;
            json uris = json::array();
            for (auto& [uri, _] : dialects) {
                uris.push_back(uri);
            }
            utsname os{};
            uname(&os);
            response = {
                {"version", 1},
                {"implementation",
                 {
                     {"language", "c++"},
                     {"name", "corvus-json-schema"},
                     {"version", std::string(cjs::detail::view(cjs_version_string()))},
                     {"homepage", "https://github.com/corvus-dotnet/Corvus.JsonSchema"},
                     {"documentation",
                      "https://github.com/corvus-dotnet/Corvus.JsonSchema/tree/main/src-rs/corvus-json-schema-capi"},
                     {"issues", "https://github.com/corvus-dotnet/Corvus.JsonSchema/issues"},
                     {"source", "https://github.com/corvus-dotnet/Corvus.JsonSchema"},
                     {"dialects", uris},
                     {"os", os.sysname},
                     {"os_version", os.release},
                     {"language_version", "C++" + std::to_string(__cplusplus / 100 % 100) + " (g++ " __VERSION__ ")"},
                 }},
            };
        } else if (cmd == "dialect") {
            if (!started) {
                throw std::runtime_error("Not started");
            }
            response = {{"ok", false}};
            for (auto& [uri, d] : dialects) {
                if (uri == request.value("dialect", "")) {
                    dialect = d;
                    response = {{"ok", true}};
                }
            }
        } else if (cmd == "run") {
            if (!started) {
                throw std::runtime_error("Not started");
            }
            const json& test_case = request["case"];
            auto registry = std::make_shared<std::map<std::string, std::string>>();
            if (test_case.contains("registry")) {
                for (auto& [uri, schema] : test_case["registry"].items()) {
                    (*registry)[strip_fragment(uri)] = schema.dump();
                }
            }
            bool annotations = request.value("output", "") == "annotations";
            try {
                cjs::options opts;
                opts.default_dialect(dialect).resolver([registry](std::string_view uri) -> std::optional<std::string> {
                    auto found = registry->find(strip_fragment(std::string(uri)));
                    if (found == registry->end()) {
                        return std::nullopt;
                    }
                    return found->second;
                });
                cjs::validator validator = cjs::validator::compile(test_case["schema"].dump(), opts);
                json results = json::array();
                for (const json& test : test_case["tests"]) {
                    try {  // an error for one instance does not stop the others
                        std::string instance = test["instance"].dump();
                        if (annotations) {
                            cjs::collector collector(cjs::results_level::verbose);
                            bool valid = validator.evaluate(instance, collector);
                            results.push_back({{"valid", valid}, {"annotations", annotations_of(json::parse(collector.annotations_json()))}});
                        } else {
                            results.push_back({{"valid", validator.is_valid(instance)}});
                        }
                    } catch (const std::exception& e) {
                        results.push_back(errored(e.what()));
                    }
                }
                response = {{"seq", request["seq"]}, {"results", results}};
            } catch (const std::exception& e) {  // every compilation failure is reported for the case
                response = errored(e.what());
                response["seq"] = request["seq"];
            }
        } else if (cmd == "stop") {
            if (!started) {
                throw std::runtime_error("Not started");
            }
            return EXIT_SUCCESS;
        } else {
            throw std::runtime_error("Unknown command " + request["cmd"].dump());
        }
        std::cout << response.dump() << std::endl;
    }
    return EXIT_SUCCESS;
}
