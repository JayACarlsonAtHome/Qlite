#include "Qlite/Setup/v002.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <sstream>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/wait.h>

extern char** environ;

namespace Setup::v002 {
namespace fs = std::filesystem;

namespace {

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::vector<std::string> split_list(const std::string& value, const char* delims) {
    std::vector<std::string> out;
    std::string token;
    for (const char c : value) {
        if (std::strchr(delims, c) != nullptr) {
            const std::string t = trim(token);
            if (!t.empty()) {
                out.push_back(t);
            }
            token.clear();
        } else {
            token.push_back(c);
        }
    }
    const std::string t = trim(token);
    if (!t.empty()) {
        out.push_back(t);
    }
    return out;
}

std::string expand_template(const std::string& tmpl, const std::string& inner) {
    std::string out;
    constexpr std::string_view marker = "{CMD}";
    std::size_t pos = 0;
    for (;;) {
        const std::size_t hit = tmpl.find(marker, pos);
        if (hit == std::string::npos) {
            out.append(tmpl, pos, std::string::npos);
            break;
        }
        out.append(tmpl, pos, hit - pos);
        out.append(inner);
        pos = hit + marker.size();
    }
    return out;
}

std::string first_line(std::string text) {
    if (const auto pos = text.find('\n'); pos != std::string::npos) {
        text.erase(pos);
    }
    return trim(std::move(text));
}

EnvSnapshot env_from_text(const std::string& text) {
    EnvSnapshot snap;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        snap.vars[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return snap;
}

std::map<std::string, std::string> compute_delta(const EnvSnapshot& baseline,
                                                 const EnvSnapshot& after) {
    std::map<std::string, std::string> delta;
    for (const auto& [key, value] : after.vars) {
        const auto it = baseline.vars.find(key);
        if (it == baseline.vars.end() || it->second != value) {
            delta[key] = value;
        }
    }
    return delta;
}

bool version_matches(const Toolchain& tc, int detected) {
    if (tc.min_version.has_value()) {
        return detected >= *tc.min_version;
    }
    return detected == tc.version_major;
}

// ---- compiled-in fallback registry (used when no config file exists) ----

Toolchain gcc_toolchain(const std::string& label, int major, const std::string& flag) {
    Toolchain tc;
    tc.label = label;
    tc.kind = "gcc";
    tc.version_major = major;
    tc.build_dir = "build-" + label;
    tc.flag = flag;
    const std::string v = std::to_string(major);
    tc.cc_names = {"g++-" + v, "g++" + v, "g++"};
    tc.activations = {
        "{CMD}",
        "gcc-toolset-" + v + "-env {CMD}",
        "scl enable gcc-toolset-" + v + " -- {CMD}",
        "bash -c 'source /opt/rh/gcc-toolset-" + v + "/enable && {CMD}'",
    };
    return tc;
}

std::vector<Toolchain> builtin_registry() {
    Toolchain clang;
    clang.label = "clang";        // results label (build_dir below stays build-clang20)
    clang.kind = "clang";
    clang.version_major = 20;
    clang.min_version = 20;
    clang.build_dir = "build-clang20";
    clang.flag = "--clang";
    clang.cc_names = {"clang++-20", "clang++20", "clang++"};
    clang.activations = {"{CMD}"};
    clang.scan_deps = {"clang-scan-deps-20", "clang-scan-deps"};

    return {
        gcc_toolchain("gcc15", 15, "--gcc15"),
        gcc_toolchain("gcc14", 14, "--force-gcc14"),
        clang,
    };
}

std::vector<Toolchain> parse_registry_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }

    std::vector<std::string> order;
    std::map<std::string, Toolchain> by_label;

    std::string line;
    while (std::getline(file, line)) {
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = trim(line);
        const auto eq = line.find('=');
        const auto dot = line.find('.');
        if (line.empty() || eq == std::string::npos || dot == std::string::npos || dot > eq) {
            continue;
        }

        const std::string label = trim(line.substr(0, dot));
        const std::string field = lower(trim(line.substr(dot + 1, eq - dot - 1)));
        const std::string value = trim(line.substr(eq + 1));
        if (label.empty() || field.empty()) {
            continue;
        }

        if (by_label.find(label) == by_label.end()) {
            order.push_back(label);
            by_label[label].label = label;
        }
        Toolchain& tc = by_label[label];

        if (field == "kind") {
            tc.kind = lower(value);
        } else if (field == "version") {
            try { tc.version_major = std::stoi(value); } catch (...) {}
        } else if (field == "min_version") {
            try { tc.min_version = std::stoi(value); } catch (...) {}
        } else if (field == "build_dir") {
            tc.build_dir = value;
        } else if (field == "flag") {
            tc.flag = value;
        } else if (field == "cc_names") {
            tc.cc_names = split_list(value, " \t,");
        } else if (field == "activations") {
            tc.activations = split_list(value, "|");
        } else if (field == "scan_deps") {
            tc.scan_deps = split_list(value, " \t,");
        } else if (field == "required_on") {
            tc.required_on = split_list(value, " \t,");
        }
    }

    std::vector<Toolchain> out;
    out.reserve(order.size());
    for (const std::string& label : order) {
        Toolchain& tc = by_label[label];
        if (tc.build_dir.empty()) {
            tc.build_dir = "build-" + label;
        }
        if (tc.activations.empty()) {
            tc.activations = {"{CMD}"};
        }
        out.push_back(std::move(tc));
    }
    return out;
}

CaptureResult run_shell(const std::string& command_line) {
    return run_capture(command_line, 30);
}

} // namespace

CaptureResult run_capture(const std::string& command_line, const int timeout_sec) {
    CaptureResult result;

    int out_pipe[2]{-1, -1};
    if (::pipe(out_pipe) != 0) {
        return result;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(out_pipe[0]);
        ::close(out_pipe[1]);
        return result;
    }

    if (pid == 0) {
        ::close(out_pipe[0]);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        if (const int devnull = ::open("/dev/null", O_WRONLY); devnull >= 0) {
            ::dup2(devnull, STDERR_FILENO);
            ::close(devnull);
        }
        ::close(out_pipe[1]);
        const char* argv[] = {"/bin/sh", "-c", command_line.c_str(), nullptr};
        ::execv("/bin/sh", const_cast<char* const*>(argv));
        std::_Exit(127);
    }

    result.started = true;
    ::close(out_pipe[1]);
    const int fd = out_pipe[0];
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(timeout_sec > 0 ? timeout_sec : 0);
    std::array<char, 4096> buf{};

    for (;;) {
        for (;;) {
            const ssize_t n = ::read(fd, buf.data(), buf.size());
            if (n > 0) {
                result.out.append(buf.data(), static_cast<std::size_t>(n));
                continue;
            }
            break; // 0 (EOF) or EAGAIN
        }

        int status = 0;
        const pid_t waited = ::waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            // Final drain after exit.
            for (;;) {
                const ssize_t n = ::read(fd, buf.data(), buf.size());
                if (n > 0) {
                    result.out.append(buf.data(), static_cast<std::size_t>(n));
                    continue;
                }
                break;
            }
            if (WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.exit_code = 128 + WTERMSIG(status);
            }
            break;
        }
        if (waited < 0) {
            break; // ECHILD or other: nothing to wait for
        }
        if (timeout_sec > 0 && std::chrono::steady_clock::now() >= deadline) {
            ::kill(pid, SIGKILL);
            ::waitpid(pid, &status, 0);
            result.exit_code = 137;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    ::close(fd);
    return result;
}

std::optional<int> parse_version_major(const std::string& text) {
    const std::size_t n = text.size();
    for (std::size_t i = 0; i < n;) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < n && std::isdigit(static_cast<unsigned char>(text[j]))) {
            ++j;
        }
        if (j < n && text[j] == '.' && j + 1 < n
            && std::isdigit(static_cast<unsigned char>(text[j + 1]))) {
            try {
                return std::stoi(text.substr(i, j - i));
            } catch (...) {
                return std::nullopt;
            }
        }
        i = j;
    }
    return std::nullopt;
}

void scrub_exported_shell_functions() {
    std::vector<std::string> names;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e) {
        const std::string entry(*e);
        const std::string name = entry.substr(0, entry.find('='));
        if (name.rfind("BASH_FUNC_", 0) == 0) {
            names.push_back(name);
        }
    }
    for (const std::string& name : names) {
        ::unsetenv(name.c_str());
    }
}

EnvSnapshot snapshot_env() {
    EnvSnapshot snap;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e) {
        const std::string entry(*e);
        const auto eq = entry.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        snap.vars[entry.substr(0, eq)] = entry.substr(eq + 1);
    }
    return snap;
}

void apply_env_delta(const std::map<std::string, std::string>& delta) {
    for (const auto& [key, value] : delta) {
        ::setenv(key.c_str(), value.c_str(), 1);
    }
}

void restore_env(const EnvSnapshot& snapshot) {
    ::clearenv();
    for (const auto& [key, value] : snapshot.vars) {
        ::setenv(key.c_str(), value.c_str(), 1);
    }
}

ResolvedToolchain resolve_toolchain(const Toolchain& tc, const EnvSnapshot& baseline) {
    ResolvedToolchain resolved;
    resolved.spec = tc;

    for (const std::string& activation : tc.activations) {
        for (const std::string& cc : tc.cc_names) {
            const CaptureResult ver =
                run_shell(expand_template(activation, cc + " --version"));
            if (!ver.started || ver.exit_code != 0 || ver.out.empty()) {
                continue;
            }
            const auto major = parse_version_major(ver.out);
            if (!major || !version_matches(tc, *major)) {
                continue;
            }

            std::string cc_path = cc;
            const CaptureResult which =
                run_shell(expand_template(activation, "command -v " + cc));
            if (which.started && which.exit_code == 0) {
                const std::string resolved_path = first_line(which.out);
                if (!resolved_path.empty()) {
                    cc_path = resolved_path;
                }
            }

            std::map<std::string, std::string> delta;
            const CaptureResult env_out = run_shell(expand_template(activation, "env"));
            if (env_out.started && env_out.exit_code == 0) {
                delta = compute_delta(baseline, env_from_text(env_out.out));
            }

            resolved.available = true;
            resolved.cc_path = cc_path;
            resolved.version_line = first_line(ver.out);
            resolved.detected_major = *major;
            resolved.matched_activation = activation;
            resolved.matched_cc = cc;
            resolved.env_delta = std::move(delta);
            return resolved;
        }
    }

    return resolved;
}

std::vector<Toolchain> load_registry(const fs::path& source_dir) {
    const fs::path local = source_dir / "compilers.local.conf";
    const fs::path tracked = source_dir / "Setup" / "compilers.conf";

    for (const fs::path& candidate : {local, tracked}) {
        std::error_code ec;
        if (fs::exists(candidate, ec)) {
            std::vector<Toolchain> tcs = parse_registry_file(candidate);
            if (!tcs.empty()) {
                return tcs;
            }
        }
    }
    return builtin_registry();
}

std::vector<ResolvedToolchain> resolve_registry(const fs::path& source_dir) {
    const std::vector<Toolchain> tcs = load_registry(source_dir);
    const EnvSnapshot baseline = snapshot_env();
    std::vector<ResolvedToolchain> out;
    out.reserve(tcs.size());
    for (const Toolchain& tc : tcs) {
        out.push_back(resolve_toolchain(tc, baseline));
    }
    return out;
}

const Toolchain* find_toolchain_by_label(const std::vector<Toolchain>& tcs,
                                         const std::string& label) {
    for (const Toolchain& tc : tcs) {
        if (tc.label == label) {
            return &tc;
        }
    }
    return nullptr;
}

const Toolchain* find_toolchain_by_flag(const std::vector<Toolchain>& tcs,
                                        const std::string& flag) {
    for (const Toolchain& tc : tcs) {
        if (!tc.flag.empty() && tc.flag == flag) {
            return &tc;
        }
    }
    return nullptr;
}

namespace {

std::string os_release_value(const std::string& key) {
    std::ifstream in("/etc/os-release");
    if (!in) {
        return {};
    }
    const std::string prefix = key + "=";
    std::string line;
    while (std::getline(in, line)) {
        if (!line.starts_with(prefix)) {
            continue;
        }
        std::string value = line.substr(prefix.size());
        if (!value.empty() && value.front() == '"') value.erase(value.begin());
        if (!value.empty() && value.back() == '"') value.pop_back();
        return trim(std::move(value));
    }
    return {};
}

std::string normalize_family(const std::string& id, const std::string& id_like,
                             const std::string& pretty) {
    const std::string id_l = lower(id);
    const std::string pretty_l = lower(pretty);
    if (id_l == "linuxmint" || pretty_l.find("linux mint") != std::string::npos) {
        return "linuxmint";
    }
    if (id_l == "fedora") {
        return "fedora";
    }
    static constexpr const char* kRhel[] = {
        "rhel", "centos", "rocky", "almalinux", "ol", "scientific", "sl"};
    for (const char* r : kRhel) {
        if (id_l == r) return "rhel";
    }
    const std::string like_l = lower(id_like);
    if (like_l.find("rhel") != std::string::npos
        || like_l.find("fedora") != std::string::npos) {
        return "rhel";
    }
    return id_l.empty() ? "unknown" : id_l;
}

std::string proc_first_value(const fs::path& path, const std::string& key) {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        if (trim(line.substr(0, colon)) == key) {
            return trim(line.substr(colon + 1));
        }
    }
    return {};
}

std::string root_storage_model() {
    std::ifstream in("/proc/mounts");
    std::string dev, mnt, rest;
    while (in >> dev >> mnt) {
        std::getline(in, rest);
        if (mnt == "/") {
            const auto slash = dev.find_last_of('/');
            return slash == std::string::npos ? dev : dev.substr(slash + 1);
        }
    }
    return {};
}

std::string utc_now() {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    ::gmtime_r(&now, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string registry_hash(const fs::path& source_dir) {
    std::string content;
    for (const fs::path& p : {source_dir / "compilers.local.conf",
                              source_dir / "Setup" / "compilers.conf"}) {
        std::ifstream in(p);
        if (in) {
            std::stringstream ss;
            ss << in.rdbuf();
            content += ss.str();
        }
    }
    if (content.empty()) {
        return "builtin";
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016zx", std::hash<std::string>{}(content));
    return buf;
}

std::string pad_right(std::string s, std::size_t width) {
    if (s.size() < width) s.append(width - s.size(), ' ');
    return s;
}

bool family_in(const std::vector<std::string>& families, const std::string& family) {
    for (const std::string& f : families) {
        if (lower(f) == family) return true;
    }
    return false;
}

} // namespace

HostSnapshot sense_host() {
    HostSnapshot h;
    h.os_id = os_release_value("ID");
    h.os_pretty = os_release_value("PRETTY_NAME");
    if (h.os_pretty.empty()) h.os_pretty = h.os_id;
    h.os_family = normalize_family(h.os_id, os_release_value("ID_LIKE"), h.os_pretty);

    if (struct utsname uts{}; ::uname(&uts) == 0) {
        h.kernel = uts.release;
    }

    h.cpu_model = proc_first_value("/proc/cpuinfo", "model name");
    {
        std::ifstream in("/proc/cpuinfo");
        std::string line;
        int cores = 0;
        double max_mhz = 0.0;
        while (std::getline(in, line)) {
            if (line.rfind("processor", 0) == 0 && line.find(':') != std::string::npos) {
                ++cores;
            } else if (line.rfind("cpu MHz", 0) == 0) {
                try { max_mhz = std::max(max_mhz, std::stod(line.substr(line.find(':') + 1))); }
                catch (...) {}
            }
        }
        h.cpu_cores = cores;
        h.cpu_mhz = static_cast<int>(max_mhz + 0.5);
    }
    if (const std::string memkb = proc_first_value("/proc/meminfo", "MemTotal"); !memkb.empty()) {
        try { h.ram_gb = static_cast<int>(std::stoll(memkb) / 1024 / 1024); } catch (...) {}
    }
    h.storage_model = root_storage_model();
    return h;
}

ReadinessReport assess_readiness(const std::vector<ResolvedToolchain>& resolved,
                                 const std::string& os_family,
                                 const fs::path& source_dir) {
    ReadinessReport rep;
    for (const ResolvedToolchain& rt : resolved) {
        if (family_in(rt.spec.required_on, os_family)) {
            rep.required.push_back(rt.spec.label);
            if (!rt.available) {
                rep.missing.push_back(rt.spec.label);
            }
        }
    }
    // Required standalone tools (recipes with `required = true`): earned by their `check`.
    for (const ProvisionRecipe& r : load_recipes(source_dir)) {
        if (!r.required) {
            continue;
        }
        rep.required.push_back(r.component);
        if (!r.check.empty()) {
            const CaptureResult c = run_capture(r.check, 20);
            if (!c.started || c.exit_code != 0) {
                rep.missing.push_back(r.component);
            }
        }
    }
    rep.ready = rep.missing.empty();
    return rep;
}

bool write_filecheck_list(const fs::path& source_dir,
                          const std::vector<ResolvedToolchain>& resolved,
                          const std::string& disk_type) {
    const HostSnapshot host = sense_host();
    const ReadinessReport rd = assess_readiness(resolved, host.os_family, source_dir);

    auto join = [](const std::vector<std::string>& v) {
        std::string s;
        for (const std::string& e : v) { if (!s.empty()) s += ", "; s += e; }
        return s.empty() ? std::string("-") : s;
    };

    std::ofstream out(source_dir / "FileCheckList.txt");
    if (!out) {
        return false;
    }

    out << "# Qlite sensed environment + readiness manifest — auto-generated"
           " (controls nothing)\n";
    out << "# Generated: " << utc_now() << "\n\n";

    out << "STATUS:   " << (rd.ready ? "READY" : "NOT READY")
        << (rd.ready ? "" : " — missing: " + join(rd.missing)) << "\n";
    out << "Platform: " << (host.os_pretty.empty() ? "-" : host.os_pretty)
        << "  (id=" << (host.os_id.empty() ? "-" : host.os_id)
        << ", family=" << host.os_family << ")\n";
    out << "Required: " << join(rd.required) << "\n";
    out << "Registry: Setup/compilers.conf  (recipe-hash " << registry_hash(source_dir) << ")\n\n";

    out << "Host\n";
    out << "  Kernel:   " << (host.kernel.empty() ? "-" : host.kernel) << "\n";
    out << "  CPU:      " << (host.cpu_model.empty() ? "-" : host.cpu_model);
    if (host.cpu_cores > 0 || host.cpu_mhz > 0) {
        out << "  (";
        if (host.cpu_cores > 0) out << host.cpu_cores << " cores";
        if (host.cpu_mhz > 0) out << (host.cpu_cores > 0 ? " @ " : "") << host.cpu_mhz << " MHz";
        out << ")";
    }
    out << "\n";
    if (host.ram_gb > 0) out << "  RAM:      " << host.ram_gb << " GB\n";
    out << "  Storage:  " << (host.storage_model.empty() ? "-" : host.storage_model)
        << "   disk_type=" << (disk_type.empty() ? "-" : disk_type) << "\n\n";

    out << "Compilers sensed\n";
    for (const ResolvedToolchain& rt : resolved) {
        const bool required = family_in(rt.spec.required_on, host.os_family);
        out << "  [" << (rt.available ? 'x' : ' ') << "] " << pad_right(rt.spec.label, 8)
            << (required ? " (required)" : "          ");
        if (rt.available) {
            if (!rt.matched_activation.empty() && rt.matched_activation != "{CMD}") {
                out << " via \"" << rt.matched_activation << "\"";
            } else {
                out << " direct";
            }
            out << " -> " << rt.cc_path;
            if (!rt.version_line.empty()) out << "   (" << rt.version_line << ")";
        } else {
            out << " not found";
        }
        out << "\n";
    }
    return true;
}

std::vector<ProvisionRecipe> load_recipes(const fs::path& source_dir) {
    std::vector<std::string> order;
    std::map<std::string, ProvisionRecipe> by_component;

    for (const fs::path& path : {source_dir / "recipes.local.conf",
                                 source_dir / "Setup" / "recipes.conf"}) {
        std::ifstream file(path);
        if (!file) {
            continue;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (const auto hash = line.find('#'); hash != std::string::npos) {
                line = line.substr(0, hash);
            }
            line = trim(line);
            const auto eq = line.find('=');
            const auto dot = line.find('.');
            if (line.empty() || eq == std::string::npos || dot == std::string::npos || dot > eq) {
                continue;
            }
            const std::string component = trim(line.substr(0, dot));
            const std::string field = lower(trim(line.substr(dot + 1, eq - dot - 1)));
            const std::string value = trim(line.substr(eq + 1));
            if (component.empty() || field.empty()) {
                continue;
            }
            if (by_component.find(component) == by_component.end()) {
                order.push_back(component);
                by_component[component].component = component;
            }
            ProvisionRecipe& r = by_component[component];
            if (field == "desc") {
                r.desc = value;
            } else if (field == "for") {
                r.for_toolchain = value;
            } else if (field == "check") {
                r.check = value;
            } else if (field == "required") {
                r.required = (value == "true" || value == "1" || value == "yes");
            } else {
                r.install_by_family[field] = value; // family -> command
            }
        }
        // First file that yields anything wins (local override replaces tracked).
        if (!order.empty()) {
            break;
        }
    }

    std::vector<ProvisionRecipe> out;
    out.reserve(order.size());
    for (const std::string& c : order) {
        out.push_back(std::move(by_component[c]));
    }
    return out;
}

ProvisionPlan plan_provision(const fs::path& source_dir,
                             const std::vector<ResolvedToolchain>& resolved,
                             const std::string& os_family) {
    ProvisionPlan plan;
    plan.family = os_family;

    const ReadinessReport rd = assess_readiness(resolved, os_family, source_dir);
    std::vector<std::string> missing = rd.missing;

    for (const ProvisionRecipe& r : load_recipes(source_dir)) {
        bool needed = false;
        if (!r.for_toolchain.empty()) {
            needed = std::find(missing.begin(), missing.end(), r.for_toolchain) != missing.end();
        } else if (!r.check.empty()) {
            const CaptureResult c = run_capture(r.check, 20);
            needed = !c.started || c.exit_code != 0;
        }
        if (!needed) {
            continue;
        }
        const auto it = r.install_by_family.find(os_family);
        if (it == r.install_by_family.end() || it->second.empty()) {
            plan.unsupported.push_back(r.component);
            continue;
        }
        plan.steps.push_back({r.component, r.desc, it->second});
    }
    return plan;
}

bool write_setup_script(const fs::path& source_dir,
                        const ProvisionPlan& plan,
                        const std::string& reinvoke_hint) {
    if (plan.steps.empty() && plan.unsupported.empty()) {
        return false;
    }

    const fs::path path = source_dir / "Setup.sh";
    std::ofstream out(path);
    if (!out) {
        return false;
    }

    out << "#!/usr/bin/env bash\n";
    out << "# Auto-generated by Qlite::Setup on " << utc_now() << " — REVIEW before running.\n";
    out << "# These commands modify your system and usually need sudo. Read them first.\n";
    out << "# Platform family: " << (plan.family.empty() ? "unknown" : plan.family) << "\n";
    out << "set -euo pipefail\n\n";

    for (const ProvisionStep& step : plan.steps) {
        out << "# " << (step.desc.empty() ? step.component : step.desc)
            << "  [" << step.component << "]\n";
        out << step.command << "\n\n";
    }

    for (const std::string& comp : plan.unsupported) {
        out << "# WARNING: '" << comp << "' is needed but recipes.conf has no entry for family '"
            << plan.family << "'. Install it manually, or add a recipe.\n";
    }
    if (!plan.unsupported.empty()) {
        out << "\n";
    }

    out << "echo\n";
    out << "echo 'Prerequisites installed. Re-run sensing to refresh readiness:'\n";
    out << "echo '  " << reinvoke_hint << "'\n";
    out.close();

    std::error_code ec;
    fs::permissions(path,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add, ec);
    return true;
}

} // namespace Setup::v002
