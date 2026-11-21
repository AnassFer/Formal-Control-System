#include "building_challenge.hpp"

#include "symctrl/factories.hpp"
#include "symctrl/model_bundle.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace challenge_app {
namespace {

constexpr char kControllerMagic[8] = {'S', 'C', 'B', 'C', '1', '\0', '\0', '\0'};

void report(const ProgressCallback& cb, int step, int total, const std::string& msg) {
    if (cb) {
        cb(step, total, msg);
    }
}

bool tryPowUint64(std::uint64_t base, std::size_t exp, std::uint64_t& out) {
    std::uint64_t result = 1;
    for (std::size_t i = 0; i < exp; ++i) {
        if (base != 0 && result > std::numeric_limits<std::uint64_t>::max() / base) {
            return false;
        }
        result *= base;
    }
    out = result;
    return true;
}

bool allRoomsInComfortZone(const symctrl::Vector& temperatures) {
    for (double temp : temperatures) {
        if (!std::isfinite(temp) || temp < kTempMin || temp > kTempMax) {
            return false;
        }
    }
    return true;
}

double inputEffort(const symctrl::Vector& u) {
    return std::accumulate(u.begin(), u.end(), 0.0);
}

// Exact integer floor division floor(a / b) for positive b.
std::int64_t floorDiv(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t q = a / b;
    const std::int64_t r = a % b;
    if (r != 0 && a < 0) {
        --q;
    }
    return q;
}

// Exact integer ceiling division ceil(a / b) for positive b.
std::int64_t ceilDiv(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t q = a / b;
    const std::int64_t r = a % b;
    if (r != 0 && a > 0) {
        ++q;
    }
    return q;
}

struct ExactLocalRange {
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    bool empty = true;
    bool hitsOut = false;
};

// Exact integer transition system implementing symctrl::ITransitionSystem with
// NumericGuarantee::ValidatedEnclosure.
//
// Model rational identities (zero floating-point operations in transition construction):
//   alpha = 450/1000, beta = 45/1000, gamma = 90/1000, Te = -1, Th = 50,
//   u_i = (3*q_i)/10 for q_i in {0, 1, 2}, diag(q_i) = (55 - 27*q_i)/1000 > 0.
//   Cell c_j in {0..9} has exact bounds [(34 + c_j)/2, (35 + c_j)/2], width eta = 1/2.
//
// 1. CooperativeMonotone (exact integer scale = 2000, cell width = 1000, domain = [0, 10000]):
//      2000*(T_i^+ - 17) = (55 - 27*q_i)*p_i + 450*(p_{i-1} + p_{i+1}) - 90 + 2700*q_i - 34000
//      evaluated at lower corners p_j = 34 + c_j and upper corners p_j = 35 + c_j.
//
// 2. BoundedDerivative (exact integer scale = 4000, cell width = 2000, domain = [0, 20000]):
//      Center x_j* = (69 + 2*c_j)/4, half-width delta_j = 1/4,
//      4000*(f_i(x*, u) - 17) = (55 - 27*q_i)*(69 + 2*c_i) + 450*(138 + 2*c_{i-1} + 2*c_{i+1})
//                               - 180 + 5400*q_i - 68000,
//      4000*(Dx * delta_x)_i  = (55 - 27*q_i) + 450 + 450 = 955 - 27*q_i.
//      Enclosure is [center - radius, center + radius].
class ExactRingBuildingTransitionSystem final : public symctrl::ITransitionSystem {
public:
    ExactRingBuildingTransitionSystem(symctrl::StateId states,
                                      symctrl::InputId inputs,
                                      std::vector<symctrl::SuccessorSet> rows,
                                      symctrl::BuildStatistics statistics)
        : states_(states),
          inputs_(inputs),
          rows_(std::move(rows)),
          statistics_(statistics) {}

    symctrl::StateId internalStateCount() const noexcept override { return states_; }
    symctrl::InputId inputCount() const noexcept override { return inputs_; }
    symctrl::NumericGuarantee numericGuarantee() const noexcept override {
        return symctrl::NumericGuarantee::ValidatedEnclosure;
    }
    const symctrl::BuildStatistics& statistics() const noexcept override { return statistics_; }

    const symctrl::SuccessorSet& successors(symctrl::StateId state,
                                            symctrl::InputId input) const override {
        if (state > states_ || input >= inputs_) {
            throw std::out_of_range("transition state/input ID");
        }
        const auto idx = symctrl::checkedSize(
            symctrl::checkedAdd(symctrl::checkedMultiply(state, inputs_), input));
        return rows_.at(idx);
    }

private:
    const symctrl::StateId states_;
    const symctrl::InputId inputs_;
    const std::vector<symctrl::SuccessorSet> rows_;
    const symctrl::BuildStatistics statistics_;
};

std::shared_ptr<const symctrl::ITransitionSystem> buildCertifiedRingBuildingModel(
    const symctrl::IStateGrid& grid,
    const symctrl::IInputCatalog& inputs,
    const symctrl::IDynamics& dynamics,
    symctrl::ReachabilityMethod method,
    symctrl::BuildOptions options) {
    using namespace symctrl;

    const auto started = std::chrono::steady_clock::now();
    const std::size_t n = grid.stateDimension();
    const StateId states = grid.internalStateCount();
    const InputId actions = inputs.inputCount();

    if (n < 3 || dynamics.stateDimension() != n || dynamics.inputDimension() != n
        || dynamics.disturbanceDimension() != 0) {
        throw std::invalid_argument("Certified ring-building builder: incompatible dynamics/grid");
    }

    // Cross-validate analytical model assumptions against the supplied IDynamics object.
    const Box fullDomain = grid.spec().domain;
    const Box emptyW{{}, {}};
    for (double uVal : kHeaterInputs) {
        const Vector uniformU(n, uVal);
        if (method == ReachabilityMethod::CooperativeMonotone) {
            if (!dynamics.cooperativeOn(fullDomain, uniformU, emptyW)) {
                throw UnsupportedCapability("Ring building dynamics failed cooperativeOn check");
            }
        } else {
            const DerivativeBounds bounds = dynamics.absoluteJacobianBounds(fullDomain, uniformU, emptyW);
            validateDerivativeBounds(bounds, n, 0);
        }
    }

    const auto pairs = checkedMultiply(checkedAdd(states, 1), actions);
    if (pairs > options.max_state_input_pairs) {
        throw std::length_error("state/input pair budget exceeded");
    }
    const auto rowCount = checkedSize(pairs);
    if (rowCount > std::vector<SuccessorSet>().max_size()) {
        throw std::length_error("transition row container limit exceeded");
    }
    auto estimatedBytes = checkedAdd(
        sizeof(ExactRingBuildingTransitionSystem),
        checkedMultiply(pairs, sizeof(SuccessorSet)));
    if (estimatedBytes > options.max_estimated_bytes) {
        throw std::length_error("transition byte budget exceeded");
    }

    std::vector<SuccessorSet> rows(rowCount, SuccessorSet{SuccessorKind::Explicit, {}});
    estimatedBytes = checkedAdd(
        sizeof(ExactRingBuildingTransitionSystem),
        checkedMultiply(rows.capacity(), sizeof(SuccessorSet)));
    if (estimatedBytes > options.max_estimated_bytes) {
        throw std::length_error("transition byte budget exceeded");
    }
    for (InputId a = 0; a < actions; ++a) {
        rows[checkedSize(a)].kind = SuccessorKind::AllStates;
    }

    // Precompute the 10 x 10 x 10 x 3 exact integer local room transitions.
    // Index: ((cPrev * 10 + cCur) * 10 + cNext) * 3 + q
    std::array<ExactLocalRange, 3000> localTable{};
    for (std::int64_t cPrev = 0; cPrev < 10; ++cPrev) {
        for (std::int64_t cCur = 0; cCur < 10; ++cCur) {
            for (std::int64_t cNext = 0; cNext < 10; ++cNext) {
                for (std::int64_t q = 0; q < 3; ++q) {
                    const std::int64_t diag = 55 - 27 * q;
                    std::int64_t yLo = 0;
                    std::int64_t yHi = 0;
                    std::int64_t cellScale = 1000;
                    std::int64_t domainUpper = 10000;

                    if (method == ReachabilityMethod::CooperativeMonotone) {
                        const std::int64_t pLoCur = 34 + cCur;
                        const std::int64_t pLoPrev = 34 + cPrev;
                        const std::int64_t pLoNext = 34 + cNext;
                        const std::int64_t pHiCur = 35 + cCur;
                        const std::int64_t pHiPrev = 35 + cPrev;
                        const std::int64_t pHiNext = 35 + cNext;

                        yLo = diag * pLoCur + 450 * (pLoPrev + pLoNext) - 90 + 2700 * q - 34000;
                        yHi = diag * pHiCur + 450 * (pHiPrev + pHiNext) - 90 + 2700 * q - 34000;
                        cellScale = 1000;
                        domainUpper = 10000;
                    } else {
                        const std::int64_t qCur = 69 + 2 * cCur;
                        const std::int64_t qPrev = 69 + 2 * cPrev;
                        const std::int64_t qNext = 69 + 2 * cNext;
                        const std::int64_t center =
                            diag * qCur + 450 * (qPrev + qNext) - 180 + 5400 * q - 68000;
                        const std::int64_t radius = diag + 900;

                        yLo = center - radius;
                        yHi = center + radius;
                        cellScale = 2000;
                        domainUpper = 20000;
                    }

                    ExactLocalRange range;
                    range.hitsOut = (yLo <= 0) || (yHi >= domainUpper);
                    const std::int64_t fRaw = ceilDiv(yLo, cellScale) - 1;
                    const std::int64_t lRaw = floorDiv(yHi, cellScale);

                    if (lRaw >= 0 && fRaw <= 9) {
                        const std::int64_t fClamped = std::max<std::int64_t>(0, fRaw);
                        const std::int64_t lClamped = std::min<std::int64_t>(9, lRaw);
                        if (fClamped <= lClamped) {
                            range.first = static_cast<std::uint32_t>(fClamped);
                            range.last = static_cast<std::uint32_t>(lClamped);
                            range.empty = false;
                        }
                    }

                    const auto idx = static_cast<std::size_t>(
                        ((cPrev * 10 + cCur) * 10 + cNext) * 3 + q);
                    localTable[idx] = range;
                }
            }
        }
    }

    // Precompute strides (dimension-0 fastest: stride[d] = 10^d) and action digits (base 3).
    std::vector<StateId> strides(n, 1);
    for (std::size_t d = 1; d < n; ++d) {
        strides[d] = checkedMultiply(strides[d - 1], kCellsPerDimension);
    }

    std::vector<std::uint8_t> actionDigits(checkedSize(checkedMultiply(actions, n)), 0);
    for (InputId a = 0; a < actions; ++a) {
        InputId rem = a;
        const auto base = checkedSize(checkedMultiply(a, n));
        for (std::size_t d = 0; d < n; ++d) {
            actionDigits[base + d] = static_cast<std::uint8_t>(rem % 3);
            rem /= 3;
        }
    }

    std::vector<std::uint8_t> stateDigits(n, 0);
    std::vector<std::uint32_t> first(n, 0);
    std::vector<std::uint32_t> last(n, 0);
    std::vector<std::uint32_t> cur(n, 0);

    BuildStatistics statistics;

    for (StateId state = 1; state <= states; ++state) {
        StateId rem = state - 1;
        for (std::size_t d = 0; d < n; ++d) {
            stateDigits[d] = static_cast<std::uint8_t>(rem % 10);
            rem /= 10;
        }

        for (InputId action = 0; action < actions; ++action) {
            const auto aBase = checkedSize(checkedMultiply(action, n));
            bool outside = false;
            bool anyEmpty = false;
            StateId internalHits = 1;

            for (std::size_t d = 0; d < n; ++d) {
                const std::size_t prev = (d == 0) ? (n - 1) : (d - 1);
                const std::size_t next = (d + 1 == n) ? 0 : (d + 1);
                const auto tblIdx = static_cast<std::size_t>(
                    ((static_cast<std::size_t>(stateDigits[prev]) * 10
                      + static_cast<std::size_t>(stateDigits[d])) * 10
                     + static_cast<std::size_t>(stateDigits[next])) * 3
                    + static_cast<std::size_t>(actionDigits[aBase + d]));
                const ExactLocalRange& lr = localTable[tblIdx];
                if (lr.hitsOut) {
                    outside = true;
                }
                if (lr.empty) {
                    anyEmpty = true;
                } else {
                    first[d] = lr.first;
                    last[d] = lr.last;
                    internalHits = checkedMultiply(internalHits, static_cast<StateId>(lr.last - lr.first + 1));
                }
            }

            if (anyEmpty) {
                internalHits = 0;
            }

            const StateId rowEdges = checkedAdd(internalHits, outside ? 1 : 0);
            if (rowEdges == 0) {
                throw std::invalid_argument("empty successor row for certified ring-building model");
            }
            const StateId totalEdges = checkedAdd(statistics.stored_edges, rowEdges);
            if (totalEdges > options.max_stored_edges) {
                throw std::length_error("stored successor budget exceeded");
            }
            const auto count = checkedSize(rowEdges);
            if (count > std::vector<StateId>().max_size()) {
                throw std::length_error("successor container limit exceeded");
            }
            const auto requestedBytes = checkedMultiply(rowEdges, sizeof(StateId));
            if (checkedAdd(estimatedBytes, requestedBytes) > options.max_estimated_bytes) {
                throw std::length_error("transition byte budget exceeded");
            }

            std::vector<StateId> ids;
            ids.reserve(count);
            const auto allocatedBytes = checkedMultiply(ids.capacity(), sizeof(StateId));
            estimatedBytes = checkedAdd(estimatedBytes, allocatedBytes);
            if (estimatedBytes > options.max_estimated_bytes) {
                throw std::length_error("transition byte budget exceeded");
            }

            if (outside) {
                ids.push_back(kOutside);
            }
            if (internalHits > 0) {
                StateId currentId = 1;
                for (std::size_t d = 0; d < n; ++d) {
                    cur[d] = first[d];
                    currentId += static_cast<StateId>(first[d]) * strides[d];
                }
                while (true) {
                    ids.push_back(currentId);
                    std::size_t d = 0;
                    while (d < n) {
                        if (cur[d] < last[d]) {
                            ++cur[d];
                            currentId += strides[d];
                            break;
                        }
                        currentId -= static_cast<StateId>(cur[d] - first[d]) * strides[d];
                        cur[d] = first[d];
                        ++d;
                    }
                    if (d == n) {
                        break;
                    }
                }
            }

            const auto index = checkedSize(checkedAdd(checkedMultiply(state, actions), action));
            rows[index].ids = std::move(ids);
            statistics.stored_edges = totalEdges;
        }
    }

    statistics.processed_pairs = pairs;
    statistics.abstraction_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();

    return std::make_shared<ExactRingBuildingTransitionSystem>(
        states, actions, std::move(rows), statistics);
}

std::filesystem::path findSiblingChallengeCli() {
    namespace fs = std::filesystem;
    std::error_code ec;
#if defined(_WIN32)
    std::vector<wchar_t> buf(32768, L'\0');
    const DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (len == 0 || len >= buf.size()) {
        return {};
    }
    const fs::path exePath(buf.data());
    const fs::path candidate = exePath.parent_path() / L"challenge_cli.exe";
    if (fs::is_regular_file(candidate, ec)) {
        return candidate;
    }
    return {};
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) {
        return {};
    }
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        return {};
    }
    const fs::path exePath = fs::canonical(fs::path(buf.c_str()), ec);
    if (ec) {
        return {};
    }
    const fs::path candidate = exePath.parent_path() / "challenge_cli";
    if (fs::is_regular_file(candidate, ec)) {
        return candidate;
    }
    return {};
#elif defined(__linux__)
    const fs::path exePath = fs::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    const fs::path candidate = exePath.parent_path() / "challenge_cli";
    if (fs::is_regular_file(candidate, ec)) {
        return candidate;
    }
    return {};
#else
    return {};
#endif
}

struct ScopedFileRemover {
    std::filesystem::path path;
    ~ScopedFileRemover() {
        if (!path.empty()) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    }
};

std::filesystem::path makeTempControllerPath() {
    static std::atomic<std::uint64_t> counter{0};
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::uint64_t seq = counter.fetch_add(1);
#if defined(_WIN32)
    const auto pid = static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    const auto pid = static_cast<std::uint64_t>(getpid());
#endif
    return std::filesystem::temp_directory_path()
        / ("symctrl-challenge-ctrl-" + std::to_string(pid) + "-"
           + std::to_string(stamp) + "-" + std::to_string(seq) + ".bin");
}

#if defined(_WIN32)
std::wstring quoteWindowsArg(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    for (auto it = arg.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == arg.end()) {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
        } else {
            out.append(backslashes, L'\\');
            out.push_back(*it);
        }
    }
    out.push_back(L'"');
    return out;
}
#endif

struct SubprocessResult {
    int exitCode = -1;
    std::string combinedOutput;
};

SubprocessResult runSubprocessAndStreamLines(
    const std::filesystem::path& exePath,
    const std::vector<std::string>& args,
    const std::function<void(const std::string&)>& onLine) {
    SubprocessResult res;
    std::string pendingLine;

    auto feedChunk = [&](const char* data, std::size_t count) {
        res.combinedOutput.append(data, count);
        for (std::size_t i = 0; i < count; ++i) {
            const char ch = data[i];
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                if (onLine) {
                    onLine(pendingLine);
                }
                pendingLine.clear();
            } else {
                pendingLine.push_back(ch);
            }
        }
    };

#if defined(_WIN32)
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        throw std::runtime_error("Failed to create pipe for isolated benchmark subprocess.");
    }
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        throw std::runtime_error("Failed to configure pipe handle inheritance.");
    }

    std::wstring cmdLine = quoteWindowsArg(exePath.wstring());
    for (const std::string& a : args) {
        const std::filesystem::path ap(a);
        cmdLine += L" ";
        cmdLine += quoteWindowsArg(ap.wstring());
    }
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        exePath.wstring().c_str(),
        mutableCmd.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        throw std::runtime_error("Failed to spawn isolated benchmark subprocess.");
    }

    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        feedChunk(buffer, static_cast<std::size_t>(bytesRead));
    }
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    res.exitCode = static_cast<int>(exitCode);
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        throw std::runtime_error("Failed to create pipe for isolated benchmark subprocess.");
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("Failed to fork isolated benchmark subprocess.");
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        std::vector<std::string> argvStorage;
        argvStorage.reserve(args.size() + 1);
        argvStorage.push_back(exePath.string());
        for (const auto& a : args) {
            argvStorage.push_back(a);
        }
        std::vector<char*> argvPtrs;
        argvPtrs.reserve(argvStorage.size() + 1);
        for (auto& s : argvStorage) {
            argvPtrs.push_back(s.data());
        }
        argvPtrs.push_back(nullptr);

        execv(exePath.c_str(), argvPtrs.data());
        _exit(127);
    }

    close(pipefd[1]);
    char buffer[4096];
    ssize_t bytesRead = 0;
    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        feedChunk(buffer, static_cast<std::size_t>(bytesRead));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        res.exitCode = WEXITSTATUS(status);
    } else {
        res.exitCode = -1;
    }
#endif

    if (!pendingLine.empty() && onLine) {
        onLine(pendingLine);
    }
    return res;
}

template <class T>
void writePod(std::ostream& out, const T& val) {
    out.write(reinterpret_cast<const char*>(&val), static_cast<std::streamsize>(sizeof(T)));
}

template <class T>
void readPod(std::istream& in, T& val) {
    in.read(reinterpret_cast<char*>(&val), static_cast<std::streamsize>(sizeof(T)));
    if (!in) {
        throw std::runtime_error("Unexpected EOF while reading binary controller file.");
    }
}

} // namespace

SafetyTablePolicy::SafetyTablePolicy(const symctrl::part2::SafetyController& controller,
                                     const symctrl::IInputCatalog& inputs,
                                     ActionStrategy strategy)
    : controller_(controller), inputs_(inputs), strategy_(strategy) {}

symctrl::PolicyDecision SafetyTablePolicy::decide(symctrl::PolicyMemory previous_memory,
                                                  symctrl::StateId current_state) const {
    const auto it = controller_.find(current_state);
    if (it == controller_.end() || it->second.empty()) {
        throw std::out_of_range("SafetyTablePolicy: no safe input defined for current state");
    }

    const auto& allowed = it->second;
    symctrl::InputId chosen = allowed.front();

    if (strategy_ == ActionStrategy::MinEnergy) {
        double bestEffort = inputEffort(inputs_.value(chosen));
        for (std::size_t i = 1; i < allowed.size(); ++i) {
            const double effort = inputEffort(inputs_.value(allowed[i]));
            if (effort < bestEffort) {
                bestEffort = effort;
                chosen = allowed[i];
            }
        }
    } else if (strategy_ == ActionStrategy::MaxHeating) {
        double bestEffort = inputEffort(inputs_.value(chosen));
        for (std::size_t i = 1; i < allowed.size(); ++i) {
            const double effort = inputEffort(inputs_.value(allowed[i]));
            if (effort > bestEffort) {
                bestEffort = effort;
                chosen = allowed[i];
            }
        }
    }

    // Safety controllers are static (memoryless, Paper §2.1.2 / Corollary 1).
    return symctrl::PolicyDecision{previous_memory, chosen};
}

ScaleEstimate estimateChallengeScale(std::size_t rooms) {
    ScaleEstimate est;
    est.rooms = rooms;
    if (rooms == 0) {
        return est;
    }

    const double nd = static_cast<double>(rooms);
    est.statesApprox = std::pow(static_cast<double>(kCellsPerDimension), nd);
    est.inputsApprox = std::pow(static_cast<double>(kHeaterInputs.size()), nd);
    est.pairsApprox = (est.statesApprox + 1.0) * est.inputsApprox;

    // Exact integer check on N=3 gives 184,832 edges (1.899^3/pair); on N=4 gives 10,442,902.
    est.estimatedEdgesApprox = est.statesApprox * est.inputsApprox * std::pow(1.90, nd);

    const double headerBytes = est.pairsApprox * static_cast<double>(sizeof(symctrl::SuccessorSet));
    const double edgeBytes = est.estimatedEdgesApprox * static_cast<double>(sizeof(symctrl::StateId));
    const double allocatorOverheadBytes = est.statesApprox * est.inputsApprox * 24.0;
    est.estimatedRamMiB = (headerBytes + edgeBytes + allocatorOverheadBytes) / (1024.0 * 1024.0);

    est.statesFitUint64 = tryPowUint64(kCellsPerDimension, rooms, est.symbolicStates);
    est.inputsFitUint64 = tryPowUint64(kHeaterInputs.size(), rooms, est.symbolicInputs);
    if (est.statesFitUint64 && est.inputsFitUint64) {
        if (est.symbolicStates < std::numeric_limits<std::uint64_t>::max()) {
            const std::uint64_t allStates = est.symbolicStates + 1;
            if (est.symbolicInputs == 0
                || allStates <= std::numeric_limits<std::uint64_t>::max() / est.symbolicInputs) {
                est.stateInputPairs = allStates * est.symbolicInputs;
                est.pairsFitUint64 = true;
            }
        }
    }

    return est;
}

MemorySnapshot queryProcessMemory() {
    MemorySnapshot snap;
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) != 0) {
        snap.currentRssMiB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        snap.peakRssMiB = static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
        snap.available = true;
    }
#elif defined(__APPLE__)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        snap.peakRssMiB = static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
        snap.available = true;
    }
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        snap.currentRssMiB = static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
        snap.available = true;
    }
#elif defined(__linux__)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        snap.peakRssMiB = static_cast<double>(usage.ru_maxrss) / 1024.0;
        snap.available = true;
    }
    std::ifstream statm("/proc/self/statm");
    std::uint64_t sizePages = 0;
    std::uint64_t residentPages = 0;
    if (statm >> sizePages >> residentPages) {
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize > 0) {
            snap.currentRssMiB = (static_cast<double>(residentPages) * static_cast<double>(pageSize))
                                 / (1024.0 * 1024.0);
            snap.available = true;
        }
    }
#endif
    return snap;
}

ChallengeSynthesisOutput runBuildingChallenge(const ChallengeConfig& config,
                                              const ProgressCallback& onProgress) {
    using namespace symctrl;
    using namespace symctrl::part2;

    if (config.rooms < 3) {
        throw std::invalid_argument("Circular building challenge requires N >= 3 rooms.");
    }

    const ScaleEstimate estimate = estimateChallengeScale(config.rooms);
    const std::uint64_t budgetMiB = config.memoryBudgetMiB == 0 ? 1ULL : config.memoryBudgetMiB;
    if (!estimate.pairsFitUint64) {
        throw std::length_error("N=" + std::to_string(config.rooms)
                                + " overflows 64-bit explicit state-input pair counts.");
    }
    if (estimate.estimatedRamMiB > static_cast<double>(budgetMiB) * 1.15) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1)
            << "N=" << config.rooms << " requires approximately " << estimate.estimatedRamMiB
            << " MiB (" << (estimate.estimatedRamMiB / 1024.0)
            << " GiB) in explicit graph form, exceeding the configured budget of "
            << budgetMiB << " MiB.";
        throw std::length_error(oss.str());
    }

    report(onProgress, 1, 5, "Building N-dimensional grid, input catalog, and ring dynamics...");

    GridSpec spec;
    spec.domain = Box{
        Vector(config.rooms, kTempMin),
        Vector(config.rooms, kTempMax)
    };
    spec.cells_per_dimension = GridIndex(config.rooms, kCellsPerDimension);
    spec.periodic = std::vector<bool>(config.rooms, false);

    auto grid = makeUniformGrid(spec);

    std::vector<Vector> inputAxes(
        config.rooms,
        Vector(kHeaterInputs.begin(), kHeaterInputs.end()));
    auto inputs = makeCartesianInputs(std::move(inputAxes));

    auto dynamics = makeRingBuildingDynamics(config.rooms);

    report(onProgress, 2, 5, "Computing certified exact-integer abstraction (T_abs)...");

    BuildOptions buildOpts;
    buildOpts.max_state_input_pairs = std::numeric_limits<std::uint64_t>::max();
    buildOpts.max_stored_edges = std::numeric_limits<std::uint64_t>::max();
    buildOpts.max_estimated_bytes = checkedMultiply(budgetMiB, 1024ULL * 1024ULL);

    auto model = buildCertifiedRingBuildingModel(
        *grid, *inputs, *dynamics, config.reachabilityMethod, buildOpts);

    report(onProgress, 3, 5, "Computing maximal safety domain R* (Algorithm 1)...");

    const auto synthStart = std::chrono::steady_clock::now();
    const StateId M = grid->internalStateCount();
    StateSet safeStates;
    for (StateId s = 1; s <= M; ++s) {
        safeStates.insert(safeStates.end(), s);
    }

    StateSet safetyDomain = computeSafetyDomain(*model, safeStates);

    report(onProgress, 4, 5, "Extracting maximal safety controller H (Theorem 2)...");
    SafetyController controller = buildSafetyController(*model, safetyDomain);

    const double synthesisSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - synthStart).count();

    report(onProgress, 5, 5, "Collecting competition metrics...");
    const MemorySnapshot mem = queryProcessMemory();
    const BuildStatistics& stats = model->statistics();

    ChallengeMetrics metrics;
    metrics.rooms = config.rooms;
    metrics.internalStates = M;
    metrics.inputCount = inputs->inputCount();
    metrics.processedPairs = stats.processed_pairs;
    metrics.storedEdges = stats.stored_edges;
    metrics.controllableStates = static_cast<std::uint64_t>(safetyDomain.size());
    metrics.controllablePercent = M > 0
        ? (100.0 * static_cast<double>(metrics.controllableStates) / static_cast<double>(M))
        : 0.0;
    metrics.abstractionSeconds = stats.abstraction_seconds;
    metrics.synthesisSeconds = synthesisSeconds;
    metrics.totalSeconds = metrics.abstractionSeconds + metrics.synthesisSeconds;
    metrics.peakMemoryMiB = mem.peakRssMiB;
    metrics.currentMemoryMiB = mem.currentRssMiB;
    metrics.memoryMeasured = mem.available;
    metrics.isolatedProcess = false;
    metrics.numericGuarantee = model->numericGuarantee();
    metrics.reachabilityMethod = config.reachabilityMethod;

    ChallengeSynthesisOutput output;
    output.grid = std::move(grid);
    output.inputs = std::move(inputs);
    output.dynamics = std::move(dynamics);
    output.model = std::move(model);
    output.safetyDomain = std::move(safetyDomain);
    output.controller = std::move(controller);
    output.metrics = metrics;
    return output;
}

void saveSafetyControllerBinary(const ChallengeSynthesisOutput& synth,
                                const std::filesystem::path& filePath) {
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open binary controller file for writing: " + filePath.string());
    }
    out.exceptions(std::ios::badbit | std::ios::failbit);
    try {
        out.write(kControllerMagic, sizeof(kControllerMagic));
        const std::uint64_t rooms = static_cast<std::uint64_t>(synth.metrics.rooms);
        const std::uint64_t domainSize = static_cast<std::uint64_t>(synth.safetyDomain.size());
        const std::uint64_t ctrlSize = static_cast<std::uint64_t>(synth.controller.size());
        writePod(out, rooms);
        writePod(out, domainSize);
        writePod(out, ctrlSize);

        for (symctrl::StateId s : synth.safetyDomain) {
            writePod(out, s);
        }
        for (const auto& entry : synth.controller) {
            writePod(out, entry.first);
            const std::uint64_t actionCount = static_cast<std::uint64_t>(entry.second.size());
            writePod(out, actionCount);
            if (!entry.second.empty()) {
                out.write(reinterpret_cast<const char*>(entry.second.data()),
                          static_cast<std::streamsize>(entry.second.size() * sizeof(symctrl::InputId)));
            }
        }
        out.flush();
        out.close();
        if (!out) {
            throw std::runtime_error("Failed while closing binary controller file.");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Binary controller write failed (" + filePath.string() + "): " + e.what());
    }
}

void loadSafetyControllerBinary(const std::filesystem::path& filePath,
                                std::size_t expectedRooms,
                                symctrl::part2::StateSet& outSafetyDomain,
                                symctrl::part2::SafetyController& outController) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open binary controller file for reading: " + filePath.string());
    }

    char magic[sizeof(kControllerMagic)]{};
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, kControllerMagic, sizeof(magic)) != 0) {
        throw std::runtime_error("Invalid binary controller header.");
    }

    std::uint64_t rooms = 0;
    std::uint64_t domainSize = 0;
    std::uint64_t ctrlSize = 0;
    readPod(in, rooms);
    readPod(in, domainSize);
    readPod(in, ctrlSize);
    if (rooms != static_cast<std::uint64_t>(expectedRooms)) {
        throw std::runtime_error("Binary controller room count mismatch.");
    }

    outSafetyDomain.clear();
    for (std::uint64_t i = 0; i < domainSize; ++i) {
        symctrl::StateId s = 0;
        readPod(in, s);
        outSafetyDomain.insert(outSafetyDomain.end(), s);
    }

    outController.clear();
    for (std::uint64_t i = 0; i < ctrlSize; ++i) {
        symctrl::StateId s = 0;
        std::uint64_t actionCount = 0;
        readPod(in, s);
        readPod(in, actionCount);
        std::vector<symctrl::InputId> actions(symctrl::checkedSize(actionCount));
        if (actionCount > 0) {
            in.read(reinterpret_cast<char*>(actions.data()),
                    static_cast<std::streamsize>(actions.size() * sizeof(symctrl::InputId)));
            if (!in) {
                throw std::runtime_error("Truncated action list in binary controller file.");
            }
        }
        outController.emplace_hint(outController.end(), s, std::move(actions));
    }
}

ChallengeSynthesisOutput runBuildingChallengeIsolated(const ChallengeConfig& config,
                                                      const ProgressCallback& onProgress) {
    const std::filesystem::path cliPath = findSiblingChallengeCli();
    if (cliPath.empty()) {
        return runBuildingChallenge(config, onProgress);
    }

    const std::string methodArg =
        (config.reachabilityMethod == symctrl::ReachabilityMethod::CooperativeMonotone)
            ? "monotone"
            : "derivative";

    ScopedFileRemover tempGuard{makeTempControllerPath()};

    ChallengeMetrics metrics;
    metrics.rooms = config.rooms;
    metrics.reachabilityMethod = config.reachabilityMethod;
    metrics.isolatedProcess = true;
    std::string workerError;

    const std::vector<std::string> args = {
        "--isolated-worker",
        std::to_string(config.rooms),
        methodArg,
        std::to_string(config.memoryBudgetMiB == 0 ? 1ULL : config.memoryBudgetMiB),
        tempGuard.path.string()
    };

    const SubprocessResult proc = runSubprocessAndStreamLines(
        cliPath, args, [&](const std::string& line) {
            if (line.rfind("PROGRESS:", 0) == 0) {
                const std::size_t p1 = line.find(':', 9);
                const std::size_t p2 = (p1 == std::string::npos) ? std::string::npos : line.find(':', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    const int step = std::stoi(line.substr(9, p1 - 9));
                    const int total = std::stoi(line.substr(p1 + 1, p2 - p1 - 1));
                    const std::string msg = line.substr(p2 + 1);
                    report(onProgress, step, total, msg);
                }
            } else if (line.rfind("METRIC:", 0) == 0) {
                const std::size_t eq = line.find('=', 7);
                if (eq != std::string::npos) {
                    const std::string key = line.substr(7, eq - 7);
                    const std::string val = line.substr(eq + 1);
                    if (key == "internal_states") metrics.internalStates = std::stoull(val);
                    else if (key == "input_count") metrics.inputCount = std::stoull(val);
                    else if (key == "processed_pairs") metrics.processedPairs = std::stoull(val);
                    else if (key == "stored_edges") metrics.storedEdges = std::stoull(val);
                    else if (key == "controllable_states") metrics.controllableStates = std::stoull(val);
                    else if (key == "controllable_percent") metrics.controllablePercent = std::stod(val);
                    else if (key == "abstraction_seconds") metrics.abstractionSeconds = std::stod(val);
                    else if (key == "synthesis_seconds") metrics.synthesisSeconds = std::stod(val);
                    else if (key == "total_seconds") metrics.totalSeconds = std::stod(val);
                    else if (key == "peak_memory_mib") metrics.peakMemoryMiB = std::stod(val);
                    else if (key == "current_memory_mib") metrics.currentMemoryMiB = std::stod(val);
                    else if (key == "memory_measured") metrics.memoryMeasured = (val == "1");
                    else if (key == "numeric_guarantee") {
                        metrics.numericGuarantee = (val == "validated_enclosure")
                            ? symctrl::NumericGuarantee::ValidatedEnclosure
                            : symctrl::NumericGuarantee::UncertifiedFloat;
                    }
                }
            } else if (line.rfind("ERROR:", 0) == 0) {
                workerError = line.substr(6);
            }
        });

    if (proc.exitCode != 0) {
        if (!workerError.empty()) {
            throw std::runtime_error(workerError);
        }
        throw std::runtime_error("Isolated benchmark subprocess failed (exit code "
                                 + std::to_string(proc.exitCode) + ").");
    }

    using namespace symctrl;
    GridSpec spec;
    spec.domain = Box{
        Vector(config.rooms, kTempMin),
        Vector(config.rooms, kTempMax)
    };
    spec.cells_per_dimension = GridIndex(config.rooms, kCellsPerDimension);
    spec.periodic = std::vector<bool>(config.rooms, false);

    ChallengeSynthesisOutput output;
    output.grid = makeUniformGrid(spec);
    std::vector<Vector> inputAxes(
        config.rooms,
        Vector(kHeaterInputs.begin(), kHeaterInputs.end()));
    output.inputs = makeCartesianInputs(std::move(inputAxes));
    output.dynamics = makeRingBuildingDynamics(config.rooms);
    output.metrics = metrics;

    loadSafetyControllerBinary(
        tempGuard.path, config.rooms, output.safetyDomain, output.controller);
    return output;
}

BuildingTrajectory simulateBuildingClosedLoop(const ChallengeSynthesisOutput& synth,
                                              const symctrl::Vector& initialTemperatures,
                                              std::size_t horizon,
                                              ActionStrategy strategy) {
    BuildingTrajectory traj;
    if (!synth.grid || !synth.inputs || !synth.dynamics) {
        traj.statusMessage = "Controller is not initialized.";
        return traj;
    }

    const std::size_t n = synth.grid->stateDimension();
    if (initialTemperatures.size() != n) {
        traj.statusMessage = "Initial temperature vector dimension does not match N.";
        return traj;
    }

    const symctrl::StateId startState = synth.grid->locate(initialTemperatures);
    if (startState == symctrl::kOutside) {
        traj.statusMessage = "Initial state T(0) is outside [17, 22]^N.";
        return traj;
    }
    if (synth.safetyDomain.count(startState) == 0) {
        traj.statusMessage = "Initial cell is outside the winning safety domain R*.";
        return traj;
    }

    SafetyTablePolicy policy(synth.controller, *synth.inputs, strategy);
    symctrl::Vector x = initialTemperatures;
    symctrl::PolicyMemory memory{};
    bool everyStepSafe = true;

    traj.steps.reserve(horizon + 1);
    for (std::size_t t = 0; t < horizon; ++t) {
        const bool safeNow = allRoomsInComfortZone(x);
        everyStepSafe = everyStepSafe && safeNow;

        const symctrl::StateId curState = synth.grid->locate(x);
        if (curState == symctrl::kOutside) {
            traj.steps.push_back({t, curState, 0, false, false, x, symctrl::Vector(n, 0.0)});
            traj.allStepsSafe = false;
            traj.statusMessage = "Safety violated (left [17, 22]^N) at step t=" + std::to_string(t) + ".";
            return traj;
        }

        const symctrl::ConcreteDecision decision =
            symctrl::refineDecision(*synth.grid, *synth.inputs, policy, x, memory);

        traj.steps.push_back({
            t,
            decision.state,
            decision.input,
            true,
            safeNow,
            x,
            decision.physical_input
        });

        x = synth.dynamics->step(x, decision.physical_input, {});
        memory = decision.next_memory;
    }

    const bool finalSafe = allRoomsInComfortZone(x);
    everyStepSafe = everyStepSafe && finalSafe;
    const symctrl::StateId finalState = synth.grid->locate(x);
    traj.steps.push_back({horizon, finalState, 0, false, finalSafe, x, symctrl::Vector(n, 0.0)});

    traj.allStepsSafe = everyStepSafe;
    traj.statusMessage = everyStepSafe
        ? ("Safe for all " + std::to_string(horizon) + " steps in [17, 22]^" + std::to_string(n) + ".")
        : "Safety bound violated during closed-loop simulation.";
    return traj;
}

void exportBuildingTrajectoryCsv(const BuildingTrajectory& trajectory,
                                 const std::filesystem::path& filePath) {
    if (trajectory.steps.empty()) {
        throw std::invalid_argument("Cannot export empty trajectory.");
    }
    const std::size_t n = trajectory.steps.front().temperatures.size();
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open CSV file: " + filePath.string());
    }
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);

    try {
        out << "t,state_id,input_id,safe";
        for (std::size_t i = 0; i < n; ++i) {
            out << ",T_" << (i + 1);
        }
        for (std::size_t i = 0; i < n; ++i) {
            out << ",u_" << (i + 1);
        }
        out << '\n';

        for (const auto& step : trajectory.steps) {
            out << step.step << ',' << step.stateId << ',' << step.inputId << ',' << (step.safe ? 1 : 0);
            for (double temp : step.temperatures) {
                out << ',' << temp;
            }
            for (double u : step.heaterInputs) {
                out << ',' << u;
            }
            out << '\n';
        }

        out.flush();
        out.close();
        if (!out || out.fail() || out.bad()) {
            throw std::runtime_error("Stream entered error state after flushing/closing.");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed while writing CSV file '" + filePath.string() + "': " + e.what());
    }
}

void exportChallengeModelPackage(const ChallengeSynthesisOutput& synth,
                                 const std::filesystem::path& directory) {
    if (!synth.grid || !synth.inputs || !synth.model) {
        throw std::invalid_argument("No explicit transition model available to export.");
    }

    symctrl::ModelMetadata meta;
    meta.model_id = "ring_building_n" + std::to_string(synth.metrics.rooms);
    meta.source = "Saoud Project - Symbolic Control (Circular Building Challenge)";
    meta.provenance = symctrl::Provenance::GeneratedPart1;
    meta.model_parameters = {
        {"N", static_cast<double>(synth.metrics.rooms)},
        {"alpha", kAlpha},
        {"beta", kBeta},
        {"gamma", kGamma},
        {"Te", kOutsideTemp},
        {"Th", kHeaterTemp}
    };
    meta.state_dimension = synth.grid->stateDimension();
    meta.input_dimension = synth.inputs->inputDimension();
    meta.disturbance_dimension = 0;
    meta.internal_states = synth.grid->internalStateCount();
    meta.input_count = synth.inputs->inputCount();
    meta.grid = synth.grid->spec();
    meta.disturbance = symctrl::Box{{}, {}};
    meta.numeric_guarantee = synth.model->numericGuarantee();

    const symctrl::ModelBundle bundle = symctrl::assembleModelBundle(
        synth.grid, synth.inputs, synth.model, std::move(meta));

    symctrl::ExportOptions exportOpts{};
    exportOpts.max_transition_rows = std::max<std::uint64_t>(
        exportOpts.max_transition_rows,
        symctrl::checkedAdd(synth.metrics.storedEdges, synth.metrics.inputCount));
    symctrl::exportModelPackage(bundle, directory, exportOpts);
}

} // namespace challenge_app
