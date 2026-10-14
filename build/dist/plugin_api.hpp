#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace astronomial {

inline constexpr std::string_view ASTRONOMIAL_PLUGIN_KEY_ID = "astronomial-ed25519-v1";
inline constexpr std::string_view REQUIRED_CERTIFICATE_FILE = ".acert";
inline constexpr std::string_view REQUIRED_CERTIFICATE_HELPER = "certencr.py";

enum class PluginHealth {
    valid,
    defective_missing_certificate,
    defective_missing_certificate_helper,
    defective_invalid_certificate
};

struct PluginValidation {
    PluginHealth health = PluginHealth::defective_invalid_certificate;
    std::string reason;
};

struct CommandArgument {
    std::string name;
    std::string description;
    bool required = false;
};

struct PluginCommand {
    std::string plugin_id;
    std::string name;
    std::string description;
    std::vector<CommandArgument> arguments;
};

struct CommandRequest {
    std::string name;
    std::vector<std::string> positional;
    std::string raw_arguments;
    std::string source;
};

struct CommandResponse {
    int exit_code = 0;
    std::string output;
    std::string error;
};

class PluginCommands {
public:
    virtual ~PluginCommands() = default;
    virtual std::vector<PluginCommand> commands() const = 0;
    virtual CommandResponse run(const CommandRequest& request) = 0;
};

using PluginCommandsFactory = PluginCommands* (*)();

} // namespace astronomial

#if defined(_WIN32)
#define ASTRONOMIAL_PLUGIN_API __declspec(dllexport)
#else
#define ASTRONOMIAL_PLUGIN_API __attribute__((visibility("default")))
#endif

extern "C" ASTRONOMIAL_PLUGIN_API astronomial::PluginCommands* astronomial_plugin_commands();
