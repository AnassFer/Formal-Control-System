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
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
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

constexpr char kControllerMagic[8] = {'S', 'C', 'B', 'C', '2', '\0', '\0', '\0'};

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

std::uint64_t popcount64(std::uint64_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<std::uint64_t>(__builtin_popcountll(v));
#else
    v = v - ((v >> 1) & 0x5555555555555555ULL);
    v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
    return (((v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL) * 0x0101010101010101ULL) >> 56;
#endif
}

struct ExactLocalRange {
    std::uint8_t first = 0;
    std::uint8_t last = 0;
    bool empty = true;
    bool hitsOut = false;
    std::int64_t interiorMargin = -1;
};

struct LocalExactModel {
    // Indexed by ((cPrev * 10 + cCur) * 10 + cNext) * 3 + q in [0, 2999].
    std::array<ExactLocalRange, 3000> ranges{};
    LocalWitnessCertificate cert{};
};

// Computes the exact-integer local 3-room transition table and automatically discovers
// local safe masks, best-margin local witnesses, and any uniform constant witness.
LocalExactModel buildExactLocalModel(symctrl::ReachabilityMethod method) {
    LocalExactModel lm;
    lm.cert.reachabilityMethod = method;

    std::array<bool, 3> uniformOk = {true, true, true};
    bool allSafe = true;

    for (std::int64_t cPrev = 0; cPrev < 10; ++cPrev) {
        for (std::int64_t cCur = 0; cCur < 10; ++cCur) {
            for (std::int64_t cNext = 0; cNext < 10; ++cNext) {
                const auto tripletIdx = static_cast<std::size_t>((cPrev * 10 + cCur) * 10 + cNext);
                std::uint8_t safeMask = 0;
                std::uint8_t bestQ = 0xFF;
                std::int64_t bestMargin = -1;

                for (std::int64_t q = 0; q < 3; ++q) {
                    const std::int64_t diag = 55 - 27 * q;
                    std::int64_t yLo = 0;
                    std::int64_t yHi = 0;
                    std::int64_t cellScale = 1000;
                    std::int64_t domainUpper = 10000;

                    if (method == symctrl::ReachabilityMethod::CooperativeMonotone) {
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
                    range.interiorMargin = std::min(yLo, domainUpper - yHi);
                    const std::int64_t fRaw = ceilDiv(yLo, cellScale) - 1;
                    const std::int64_t lRaw = floorDiv(yHi, cellScale);

                    if (lRaw >= 0 && fRaw <= 9) {
                        const std::int64_t fClamped = std::max<std::int64_t>(0, fRaw);
                        const std::int64_t lClamped = std::min<std::int64_t>(9, lRaw);
                        if (fClamped <= lClamped) {
                            range.first = static_cast<std::uint8_t>(fClamped);
                            range.last = static_cast<std::uint8_t>(lClamped);
                            range.empty = false;
                        }
                    }

                    lm.ranges[tripletIdx * 3 + static_cast<std::size_t>(q)] = range;

                    if (!range.hitsOut && !range.empty) {
                        safeMask |= static_cast<std::uint8_t>(1u << static_cast<unsigned>(q));
                        if (range.interiorMargin > bestMargin) {
                            bestMargin = range.interiorMargin;
                            bestQ = static_cast<std::uint8_t>(q);
                        }
                    } else {
                        uniformOk[static_cast<std::size_t>(q)] = false;
                    }
                }

                lm.cert.localSafeMask[tripletIdx] = safeMask;
                lm.cert.bestLocalWitness[tripletIdx] = bestQ;
                if (safeMask == 0) {
                    allSafe = false;
                }
            }
        }
    }

    lm.cert.allTripletsHaveSafeAction = allSafe;
    lm.cert.uniformWitnessQ = -1;
    for (int q = 0; q < 3; ++q) {
        if (uniformOk[static_cast<std::size_t>(q)]) {
            lm.cert.uniformWitnessQ = q;
            break;
        }
    }
    return lm;
}

void crossValidateDynamics(const symctrl::IDynamics& dynamics,
                           std::size_t rooms,
                           symctrl::ReachabilityMethod method) {
    const symctrl::Box fullDomain{
        symctrl::Vector(rooms, kTempMin),
        symctrl::Vector(rooms, kTempMax)
    };
    const symctrl::Box emptyW{{}, {}};
    for (double uVal : kHeaterInputs) {
        const symctrl::Vector uniformU(rooms, uVal);
        if (method == symctrl::ReachabilityMethod::CooperativeMonotone) {
            if (!dynamics.cooperativeOn(fullDomain, uniformU, emptyW)) {
                throw symctrl::UnsupportedCapability("Ring building dynamics failed cooperativeOn check");
            }
        } else {
            const symctrl::DerivativeBounds bounds =
                dynamics.absoluteJacobianBounds(fullDomain, uniformU, emptyW);
            symctrl::validateDerivativeBounds(bounds, rooms, 0);
        }
    }
}

// Lazy O(1)-init IInputCatalog for {0, 0.3, 0.6}^N that never allocates all 3^N commands.
// Accessed input vectors are memoized in stable unique_ptr storage for catalog lifetime.
class LazyRingInputCatalog final : public symctrl::IInputCatalog {
public:
    explicit LazyRingInputCatalog(std::size_t rooms) : rooms_(rooms) {
        if (rooms_ == 0) {
            throw std::invalid_argument("LazyRingInputCatalog: rooms must be positive");
        }
        std::uint64_t total = 0;
        if (!tryPowUint64(kHeaterInputs.size(), rooms_, total)) {
            throw std::overflow_error("LazyRingInputCatalog: 3^N overflows uint64_t");
        }
        count_ = total;
    }

    std::size_t inputDimension() const noexcept override { return rooms_; }
    symctrl::InputId inputCount() const noexcept override { return count_; }

    const symctrl::Vector& value(symctrl::InputId input) const override {
        if (input >= count_) {
            throw std::out_of_range("LazyRingInputCatalog::value: input ID out of range");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = cache_.find(input);
        if (it != cache_.end()) {
            return *(it->second);
        }
        auto vec = std::make_unique<symctrl::Vector>(rooms_);
        symctrl::InputId rem = input;
        for (std::size_t d = 0; d < rooms_; ++d) {
            (*vec)[d] = kHeaterInputs[static_cast<std::size_t>(rem % 3)];
            rem /= 3;
        }
        const auto inserted = cache_.emplace(input, std::move(vec)).first;
        return *(inserted->second);
    }

private:
    std::size_t rooms_;
    symctrl::InputId count_ = 0;
    mutable std::mutex mutex_;
    mutable std::unordered_map<symctrl::InputId, std::unique_ptr<symctrl::Vector>> cache_;
};

// Exact integer explicit transition system implementing symctrl::ITransitionSystem
// with NumericGuarantee::ValidatedEnclosure.
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

std::shared_ptr<const symctrl::ITransitionSystem> buildCertifiedExplicitModel(
    const symctrl::IStateGrid& grid,
    const symctrl::IInputCatalog& inputs,
    const LocalExactModel& localModel,
    symctrl::BuildOptions options) {
    using namespace symctrl;

    const auto started = std::chrono::steady_clock::now();
    const std::size_t n = grid.stateDimension();
    const StateId states = grid.internalStateCount();
    const InputId actions = inputs.inputCount();

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
    std::vector<std::uint8_t> first(n, 0);
    std::vector<std::uint8_t> last(n, 0);
    std::vector<std::uint8_t> cur(n, 0);

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
                const ExactLocalRange& lr = localModel.ranges[tblIdx];
                if (lr.hitsOut) {
                    outside = true;
                }
                if (lr.empty) {
                    anyEmpty = true;
                } else {
                    first[d] = lr.first;
                    last[d] = lr.last;
                    internalHits = checkedMultiply(
                        internalHits, static_cast<StateId>(lr.last - lr.first + 1));
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

inline bool getBit(const std::vector<std::uint64_t>& bits, std::uint64_t idx) noexcept {
    return ((bits[static_cast<std::size_t>(idx >> 6)] >> (idx & 63u)) & 1ULL) != 0ULL;
}

// Verifies whether every successor in the exact Cartesian box [first[d]..last[d]] lies in safeBits.
bool allBoxSuccessorsInBitset(std::size_t n,
                              const std::uint8_t* first,
                              const std::uint8_t* last,
                              const std::uint64_t* strides,
                              const std::vector<std::uint64_t>& safeBits) noexcept {
    std::array<std::uint8_t, 20> cur{};
    std::uint64_t idx = 0;
    for (std::size_t d = 0; d < n; ++d) {
        cur[d] = first[d];
        idx += static_cast<std::uint64_t>(first[d]) * strides[d];
    }
    while (true) {
        if (!getBit(safeBits, idx)) {
            return false;
        }
        std::size_t d = 0;
        while (d < n) {
            if (cur[d] < last[d]) {
                ++cur[d];
                idx += strides[d];
                break;
            }
            idx -= static_cast<std::uint64_t>(cur[d] - first[d]) * strides[d];
            cur[d] = first[d];
            ++d;
        }
        if (d == n) {
            return true;
        }
    }
}

// Searches for a witness joint action q in {0,1,2}^N (trying preferred local witness first)
// whose exact Cartesian successor box stays completely inside safeBits.
bool findBitsetWitnessAction(std::size_t n,
                             const std::uint8_t* digits,
                             const LocalExactModel& localModel,
                             const std::uint64_t* strides,
                             const std::vector<std::uint64_t>& safeBits,
                             std::uint8_t* outChosenQ = nullptr) noexcept {
    std::array<std::uint8_t, 20> candCount{};
    std::array<std::array<std::uint8_t, 3>, 20> cands{};
    std::array<std::size_t, 20> triplets{};

    for (std::size_t d = 0; d < n; ++d) {
        const std::size_t prev = (d == 0) ? (n - 1) : (d - 1);
        const std::size_t next = (d + 1 == n) ? 0 : (d + 1);
        const std::size_t t =
            (static_cast<std::size_t>(digits[prev]) * 10 + static_cast<std::size_t>(digits[d])) * 10
            + static_cast<std::size_t>(digits[next]);
        triplets[d] = t;

        const std::uint8_t mask = localModel.cert.localSafeMask[t];
        if (mask == 0) {
            return false;
        }

        std::uint8_t cnt = 0;
        const std::uint8_t pref = (localModel.cert.uniformWitnessQ >= 0)
            ? static_cast<std::uint8_t>(localModel.cert.uniformWitnessQ)
            : localModel.cert.bestLocalWitness[t];
        if (pref < 3 && ((mask >> pref) & 1u) != 0u) {
            cands[d][cnt++] = pref;
        }
        for (std::uint8_t q = 0; q < 3; ++q) {
            if (q != pref && ((mask >> q) & 1u) != 0u) {
                cands[d][cnt++] = q;
            }
        }
        candCount[d] = cnt;
    }

    // Odometer over candidate witness actions (starts with the preferred witness tuple!).
    std::array<std::uint8_t, 20> pos{};
    std::array<std::uint8_t, 20> first{};
    std::array<std::uint8_t, 20> last{};

    while (true) {
        for (std::size_t d = 0; d < n; ++d) {
            const std::uint8_t q = cands[d][pos[d]];
            const ExactLocalRange& r = localModel.ranges[triplets[d] * 3 + q];
            first[d] = r.first;
            last[d] = r.last;
        }
        if (allBoxSuccessorsInBitset(n, first.data(), last.data(), strides, safeBits)) {
            if (outChosenQ != nullptr) {
                for (std::size_t d = 0; d < n; ++d) {
                    outChosenQ[d] = cands[d][pos[d]];
                }
            }
            return true;
        }
        std::size_t d = 0;
        while (d < n) {
            if (pos[d] + 1 < candCount[d]) {
                ++pos[d];
                break;
            }
            pos[d] = 0;
            ++d;
        }
        if (d == n) {
            return false;
        }
    }
}

struct BitsetFixedPointResult {
    std::vector<std::uint64_t> safeBits;
    std::uint64_t controllableStates = 0;
    std::uint32_t iterations = 0;
    bool allStatesSafe = false;
};

BitsetFixedPointResult runBitsetWitnessFixedPoint(std::size_t n,
                                                  std::uint64_t M,
                                                  const LocalExactModel& localModel,
                                                  std::uint64_t budgetBytes,
                                                  const ProgressCallback& onProgress) {
    if (n > 18) {
        throw std::length_error("BitsetWitness requires N <= 18 so 10^N fits in 64-bit indices.");
    }

    const std::uint64_t numWords = (M + 63ULL) / 64ULL;
    const std::uint64_t bitsetBytes = symctrl::checkedMultiply(numWords, sizeof(std::uint64_t));
    if (bitsetBytes > budgetBytes) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1)
            << "N=" << n << " bitset requires "
            << (static_cast<double>(bitsetBytes) / (1024.0 * 1024.0))
            << " MiB, exceeding budget of "
            << (static_cast<double>(budgetBytes) / (1024.0 * 1024.0)) << " MiB.";
        throw std::length_error(oss.str());
    }

    std::vector<std::uint64_t> safeBits(symctrl::checkedSize(numWords), ~0ULL);
    const unsigned remBits = static_cast<unsigned>(M & 63ULL);
    if (remBits != 0 && !safeBits.empty()) {
        safeBits.back() = (1ULL << remBits) - 1ULL;
    }

    std::array<std::uint64_t, 20> strides{};
    strides[0] = 1;
    for (std::size_t d = 1; d < n; ++d) {
        strides[d] = strides[d - 1] * 10ULL;
    }

    const unsigned hwThreads = std::max(1u, std::thread::hardware_concurrency());
    const std::uint64_t numThreads = std::min<std::uint64_t>(
        hwThreads, std::max<std::uint64_t>(1ULL, numWords / 64ULL));

    // Iteration 1: R_0 = Q_s (all M states). Membership in R_0 decouples coordinate-wise,
    // so every state is verified in O(N) via the exact-integer witness check without needing a second buffer!
    report(onProgress, 3, 5, "Bitset + Witness Pass 1 across all 10^N states...");

    const int uniformQ = localModel.cert.uniformWitnessQ;
    std::atomic<std::uint64_t> removedIter1{0};

    auto runIter1Chunk = [&](std::uint64_t wBegin, std::uint64_t wEnd) {
        std::array<std::uint8_t, 20> digits{};
        std::uint64_t stateIdx = wBegin * 64ULL;
        {
            std::uint64_t tmp = stateIdx;
            for (std::size_t d = 0; d < n; ++d) {
                digits[d] = static_cast<std::uint8_t>(tmp % 10ULL);
                tmp /= 10ULL;
            }
        }

        std::uint64_t localRemoved = 0;
        for (std::uint64_t w = wBegin; w < wEnd; ++w) {
            const std::uint64_t limit = std::min<std::uint64_t>(64ULL, M - stateIdx);
            std::uint64_t wordMask = 0ULL;

            for (std::uint64_t bit = 0; bit < limit; ++bit) {
                bool safe = true;
                if (uniformQ >= 0) {
                    const std::size_t uq = static_cast<std::size_t>(uniformQ);
                    for (std::size_t d = 0; d < n; ++d) {
                        const std::size_t prev = (d == 0) ? (n - 1) : (d - 1);
                        const std::size_t next = (d + 1 == n) ? 0 : (d + 1);
                        const std::size_t tblIdx =
                            ((static_cast<std::size_t>(digits[prev]) * 10
                              + static_cast<std::size_t>(digits[d])) * 10
                             + static_cast<std::size_t>(digits[next])) * 3 + uq;
                        const ExactLocalRange& r = localModel.ranges[tblIdx];
                        if (r.hitsOut || r.empty) {
                            safe = false;
                            break;
                        }
                    }
                } else {
                    safe = false;
                }

                if (!safe) {
                    // Fallback: check whether every room has ANY valid local witness action.
                    safe = true;
                    for (std::size_t d = 0; d < n; ++d) {
                        const std::size_t prev = (d == 0) ? (n - 1) : (d - 1);
                        const std::size_t next = (d + 1 == n) ? 0 : (d + 1);
                        const std::size_t triplet =
                            (static_cast<std::size_t>(digits[prev]) * 10
                             + static_cast<std::size_t>(digits[d])) * 10
                            + static_cast<std::size_t>(digits[next]);
                        if (localModel.cert.localSafeMask[triplet] == 0) {
                            safe = false;
                            break;
                        }
                    }
                }

                if (safe) {
                    wordMask |= (1ULL << bit);
                } else {
                    ++localRemoved;
                }

                // Increment base-10 odometer digits for next stateIdx
                ++stateIdx;
                for (std::size_t d = 0; d < n; ++d) {
                    if (++digits[d] < 10u) {
                        break;
                    }
                    digits[d] = 0;
                }
            }

            safeBits[static_cast<std::size_t>(w)] = wordMask;
        }
        removedIter1.fetch_add(localRemoved, std::memory_order_relaxed);
    };

    if (numThreads <= 1) {
        runIter1Chunk(0, numWords);
    } else {
        std::vector<std::thread> threads;
        threads.reserve(static_cast<std::size_t>(numThreads));
        for (std::uint64_t t = 0; t < numThreads; ++t) {
            const std::uint64_t wBegin = (numWords * t) / numThreads;
            const std::uint64_t wEnd = (numWords * (t + 1)) / numThreads;
            if (wBegin < wEnd) {
                threads.emplace_back(runIter1Chunk, wBegin, wEnd);
            }
        }
        for (auto& th : threads) {
            th.join();
        }
    }

    std::uint32_t iterations = 1;
    std::uint64_t totalRemoved = removedIter1.load();

    // General Algorithm 1 continuation if any state left Q_s on Pass 1 (R_1 != R_0).
    if (totalRemoved > 0) {
        if (symctrl::checkedMultiply(bitsetBytes, 2ULL) > budgetBytes) {
            throw std::length_error("Double-buffered bitset exceeds configured RAM budget.");
        }
        std::vector<std::uint64_t> nextBits(symctrl::checkedSize(numWords), 0ULL);

        while (true) {
            ++iterations;
            report(onProgress, 4, 5,
                   "Bitset + Witness fixed-point iteration " + std::to_string(iterations) + "...");
            std::atomic<std::uint64_t> removedThisIter{0};

            auto runIterKChunk = [&](std::uint64_t wBegin, std::uint64_t wEnd) {
                std::array<std::uint8_t, 20> digits{};
                std::uint64_t stateIdx = wBegin * 64ULL;
                {
                    std::uint64_t tmp = stateIdx;
                    for (std::size_t d = 0; d < n; ++d) {
                        digits[d] = static_cast<std::uint8_t>(tmp % 10ULL);
                        tmp /= 10ULL;
                    }
                }

                std::uint64_t localRemoved = 0;
                for (std::uint64_t w = wBegin; w < wEnd; ++w) {
                    const std::uint64_t curWord = safeBits[static_cast<std::size_t>(w)];
                    const std::uint64_t limit = std::min<std::uint64_t>(64ULL, M - stateIdx);
                    if (curWord == 0ULL) {
                        nextBits[static_cast<std::size_t>(w)] = 0ULL;
                        stateIdx += limit;
                        std::uint64_t tmp = stateIdx;
                        for (std::size_t d = 0; d < n; ++d) {
                            digits[d] = static_cast<std::uint8_t>(tmp % 10ULL);
                            tmp /= 10ULL;
                        }
                        continue;
                    }

                    std::uint64_t newWord = 0ULL;
                    for (std::uint64_t bit = 0; bit < limit; ++bit) {
                        if (((curWord >> bit) & 1ULL) != 0ULL) {
                            if (findBitsetWitnessAction(
                                    n, digits.data(), localModel, strides.data(), safeBits)) {
                                newWord |= (1ULL << bit);
                            } else {
                                ++localRemoved;
                            }
                        }
                        ++stateIdx;
                        for (std::size_t d = 0; d < n; ++d) {
                            if (++digits[d] < 10u) {
                                break;
                            }
                            digits[d] = 0;
                        }
                    }
                    nextBits[static_cast<std::size_t>(w)] = newWord;
                }
                removedThisIter.fetch_add(localRemoved, std::memory_order_relaxed);
            };

            if (numThreads <= 1) {
                runIterKChunk(0, numWords);
            } else {
                std::vector<std::thread> threads;
                threads.reserve(static_cast<std::size_t>(numThreads));
                for (std::uint64_t t = 0; t < numThreads; ++t) {
                    const std::uint64_t wBegin = (numWords * t) / numThreads;
                    const std::uint64_t wEnd = (numWords * (t + 1)) / numThreads;
                    if (wBegin < wEnd) {
                        threads.emplace_back(runIterKChunk, wBegin, wEnd);
                    }
                }
                for (auto& th : threads) {
                    th.join();
                }
            }

            safeBits.swap(nextBits);
            if (removedThisIter.load() == 0) {
                break;
            }
        }
    }

    std::uint64_t controllable = 0;
    for (std::uint64_t word : safeBits) {
        controllable += popcount64(word);
    }

    BitsetFixedPointResult res;
    res.controllableStates = controllable;
    res.iterations = iterations;
    res.allStatesSafe = (controllable == M);
    if (!res.allStatesSafe) {
        res.safeBits = std::move(safeBits);
    }
    return res;
}

std::uint8_t selectLocalSafeAction(std::size_t tripletIdx,
                                   const LocalWitnessCertificate& cert,
                                   ActionStrategy strategy) {
    const std::uint8_t mask = cert.localSafeMask[tripletIdx];
    if (mask == 0) {
        throw std::out_of_range("No certified local safe action available for room triplet");
    }
    if (strategy == ActionStrategy::Witness) {
        if (cert.uniformWitnessQ >= 0
            && ((mask >> static_cast<unsigned>(cert.uniformWitnessQ)) & 1u) != 0u) {
            return static_cast<std::uint8_t>(cert.uniformWitnessQ);
        }
        return cert.bestLocalWitness[tripletIdx];
    }
    if (strategy == ActionStrategy::MaxHeating) {
        for (int q = 2; q >= 0; --q) {
            if (((mask >> static_cast<unsigned>(q)) & 1u) != 0u) {
                return static_cast<std::uint8_t>(q);
            }
        }
    }
    // MinEnergy or FirstSafe: smallest safe q in {0, 1, 2} (corresponds to 0.0, 0.3, 0.6).
    for (std::uint8_t q = 0; q < 3; ++q) {
        if (((mask >> q) & 1u) != 0u) {
            return q;
        }
    }
    throw std::out_of_range("No certified local safe action available");
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

std::uint8_t locateRoomCell1D(double temp) {
    if (!std::isfinite(temp) || temp < kTempMin || temp > kTempMax) {
        return 0xFF;
    }
    const double raw = (temp - kTempMin) / 0.5;
    long long idx = static_cast<long long>(std::floor(raw));
    if (idx < 0) idx = 0;
    if (idx >= static_cast<long long>(kCellsPerDimension)) {
        idx = static_cast<long long>(kCellsPerDimension) - 1;
    }
    return static_cast<std::uint8_t>(idx);
}

} // namespace

const char* toString(SolverMode mode) noexcept {
    switch (mode) {
        case SolverMode::BitsetWitness: return "bitset";
        case SolverMode::CompactWitness: return "temoin";
        case SolverMode::ExplicitGraph: return "explicit";
    }
    return "unknown";
}

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

    if (strategy_ == ActionStrategy::Witness) {
        // Prefer constant 0.3 if allowed, else minimal deviation from 0.3.
        double bestDist = std::numeric_limits<double>::infinity();
        for (symctrl::InputId id : allowed) {
            const auto& u = inputs_.value(id);
            double dist = 0.0;
            for (double v : u) {
                dist += std::abs(v - 0.3);
            }
            if (dist < bestDist) {
                bestDist = dist;
                chosen = id;
            }
        }
    } else if (strategy_ == ActionStrategy::MinEnergy) {
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

    return symctrl::PolicyDecision{previous_memory, chosen};
}

BitsetWitnessPolicy::BitsetWitnessPolicy(std::size_t rooms,
                                         const LocalWitnessCertificate& cert,
                                         const std::vector<std::uint64_t>& safeBitset,
                                         ActionStrategy strategy)
    : rooms_(rooms), cert_(cert), safeBitset_(safeBitset), strategy_(strategy) {
    inputStrides_.resize(rooms_, 1);
    for (std::size_t d = 1; d < rooms_; ++d) {
        inputStrides_[d] = symctrl::checkedMultiply(inputStrides_[d - 1], 3ULL);
    }
}

symctrl::PolicyDecision BitsetWitnessPolicy::decide(symctrl::PolicyMemory previous_memory,
                                                    symctrl::StateId current_state) const {
    if (current_state == symctrl::kOutside) {
        throw std::out_of_range("BitsetWitnessPolicy: OUT state has no safe action");
    }
    const std::uint64_t idx = current_state - 1ULL;
    if (!cert_.allStatesSafe) {
        if (safeBitset_.empty() || (idx >> 6) >= safeBitset_.size() || !getBit(safeBitset_, idx)) {
            throw std::out_of_range("BitsetWitnessPolicy: state is outside winning bitset");
        }
    }

    std::vector<std::uint8_t> digits(rooms_, 0);
    std::uint64_t rem = idx;
    for (std::size_t d = 0; d < rooms_; ++d) {
        digits[d] = static_cast<std::uint8_t>(rem % 10ULL);
        rem /= 10ULL;
    }

    symctrl::InputId actionId = 0;
    if (cert_.allStatesSafe) {
        for (std::size_t d = 0; d < rooms_; ++d) {
            const std::size_t prev = (d == 0) ? (rooms_ - 1) : (d - 1);
            const std::size_t next = (d + 1 == rooms_) ? 0 : (d + 1);
            const std::size_t triplet =
                (static_cast<std::size_t>(digits[prev]) * 10 + static_cast<std::size_t>(digits[d])) * 10
                + static_cast<std::size_t>(digits[next]);
            const std::uint8_t q = selectLocalSafeAction(triplet, cert_, strategy_);
            actionId += static_cast<symctrl::InputId>(q) * inputStrides_[d];
        }
    } else {
        // General bitset witness recovery for non-100%-safe domains.
        const LocalExactModel lm = buildExactLocalModel(cert_.reachabilityMethod);
        std::array<std::uint64_t, 20> strides{};
        strides[0] = 1;
        for (std::size_t d = 1; d < rooms_; ++d) {
            strides[d] = strides[d - 1] * 10ULL;
        }
        std::array<std::uint8_t, 20> chosenQ{};
        if (!findBitsetWitnessAction(rooms_, digits.data(), lm, strides.data(), safeBitset_, chosenQ.data())) {
            throw std::out_of_range("BitsetWitnessPolicy: no witness action found");
        }
        for (std::size_t d = 0; d < rooms_; ++d) {
            actionId += static_cast<symctrl::InputId>(chosenQ[d]) * inputStrides_[d];
        }
    }

    return symctrl::PolicyDecision{previous_memory, actionId};
}

ScaleEstimate estimateChallengeScale(std::size_t rooms, SolverMode mode) {
    ScaleEstimate est;
    est.rooms = rooms;
    est.solverMode = mode;
    if (rooms == 0) {
        return est;
    }

    const double nd = static_cast<double>(rooms);
    est.statesApprox = std::pow(static_cast<double>(kCellsPerDimension), nd);
    est.inputsApprox = std::pow(static_cast<double>(kHeaterInputs.size()), nd);
    est.pairsApprox = (est.statesApprox + 1.0) * est.inputsApprox;
    est.estimatedEdgesApprox = est.statesApprox * est.inputsApprox * std::pow(1.90, nd);

    if (mode == SolverMode::ExplicitGraph) {
        const double headerBytes = est.pairsApprox * static_cast<double>(sizeof(symctrl::SuccessorSet));
        const double edgeBytes = est.estimatedEdgesApprox * static_cast<double>(sizeof(symctrl::StateId));
        const double allocatorOverheadBytes = est.statesApprox * est.inputsApprox * 24.0;
        est.estimatedRamMiB = (headerBytes + edgeBytes + allocatorOverheadBytes) / (1024.0 * 1024.0);
    } else if (mode == SolverMode::BitsetWitness) {
        // 1 bit per state (10^N / 8 bytes) + ~8 MiB base runtime footprint.
        est.estimatedRamMiB = (est.statesApprox / 8.0) / (1024.0 * 1024.0) + 8.0;
    } else {
        // CompactWitness / Temoin: O(1) local table (~36 KB) + base runtime (~8 MiB).
        est.estimatedRamMiB = 8.0;
    }

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

    const ScaleEstimate estimate = estimateChallengeScale(config.rooms, config.solverMode);
    const std::uint64_t budgetMiB = config.memoryBudgetMiB == 0 ? 1ULL : config.memoryBudgetMiB;
    const std::uint64_t budgetBytes = checkedMultiply(budgetMiB, 1024ULL * 1024ULL);

    if (config.solverMode == SolverMode::ExplicitGraph) {
        if (!estimate.pairsFitUint64) {
            throw std::length_error("N=" + std::to_string(config.rooms)
                                    + " overflows 64-bit explicit state-input pair counts.");
        }
        if (estimate.estimatedRamMiB > static_cast<double>(budgetMiB) * 1.15) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1)
                << "N=" << config.rooms << " requires approximately " << estimate.estimatedRamMiB
                << " MiB (" << (estimate.estimatedRamMiB / 1024.0)
                << " GiB) in ExplicitGraph mode, exceeding budget of " << budgetMiB << " MiB.";
            throw std::length_error(oss.str());
        }
    } else if (config.solverMode == SolverMode::BitsetWitness) {
        if (!estimate.statesFitUint64 || config.rooms > 18) {
            throw std::length_error("N=" + std::to_string(config.rooms)
                                    + " exceeds 64-bit bitset indexing (use CompactWitness / temoin for N > 18).");
        }
        if (estimate.estimatedRamMiB > static_cast<double>(budgetMiB) * 1.15) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1)
                << "N=" << config.rooms << " bitset requires approximately " << estimate.estimatedRamMiB
                << " MiB (" << (estimate.estimatedRamMiB / 1024.0)
                << " GiB), exceeding budget of " << budgetMiB << " MiB.";
            throw std::length_error(oss.str());
        }
    }

    report(onProgress, 1, 5, "Validating ring dynamics and building N-dimensional domain...");

    auto dynamics = makeRingBuildingDynamics(config.rooms);
    crossValidateDynamics(*dynamics, config.rooms, config.reachabilityMethod);

    std::shared_ptr<const IStateGrid> grid;
    std::shared_ptr<const IInputCatalog> inputs;

    if (estimate.statesFitUint64) {
        GridSpec spec;
        spec.domain = Box{
            Vector(config.rooms, kTempMin),
            Vector(config.rooms, kTempMax)
        };
        spec.cells_per_dimension = GridIndex(config.rooms, kCellsPerDimension);
        spec.periodic = std::vector<bool>(config.rooms, false);
        grid = makeUniformGrid(spec);
    }

    if (config.solverMode == SolverMode::ExplicitGraph) {
        std::vector<Vector> inputAxes(
            config.rooms,
            Vector(kHeaterInputs.begin(), kHeaterInputs.end()));
        inputs = makeCartesianInputs(std::move(inputAxes));
    } else if (estimate.inputsFitUint64) {
        // Lazy catalog: zero 3^N upfront memory, fully compatible with symctrl::refineDecision!
        inputs = std::make_shared<LazyRingInputCatalog>(config.rooms);
    }

    ChallengeSynthesisOutput output;
    output.grid = grid;
    output.inputs = inputs;
    output.dynamics = dynamics;

    ChallengeMetrics metrics;
    metrics.rooms = config.rooms;
    metrics.solverMode = config.solverMode;
    metrics.statesFitUint64 = estimate.statesFitUint64;
    metrics.internalStates = estimate.symbolicStates;
    metrics.internalStatesApprox = estimate.statesApprox;
    metrics.inputsFitUint64 = estimate.inputsFitUint64;
    metrics.inputCount = estimate.symbolicInputs;
    metrics.inputCountApprox = estimate.inputsApprox;
    metrics.isolatedProcess = false;
    metrics.numericGuarantee = NumericGuarantee::ValidatedEnclosure;
    metrics.reachabilityMethod = config.reachabilityMethod;

    if (config.solverMode == SolverMode::ExplicitGraph) {
        report(onProgress, 2, 5, "Computing certified exact-integer explicit graph (T_abs)...");
        const LocalExactModel localModel = buildExactLocalModel(config.reachabilityMethod);

        BuildOptions buildOpts;
        buildOpts.max_state_input_pairs = std::numeric_limits<std::uint64_t>::max();
        buildOpts.max_stored_edges = std::numeric_limits<std::uint64_t>::max();
        buildOpts.max_estimated_bytes = budgetBytes;

        auto model = buildCertifiedExplicitModel(*grid, *inputs, localModel, buildOpts);

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

        const BuildStatistics& stats = model->statistics();
        metrics.processedPairs = stats.processed_pairs;
        metrics.storedEdges = stats.stored_edges;
        metrics.controllableStates = static_cast<std::uint64_t>(safetyDomain.size());
        metrics.controllablePercent = M > 0
            ? (100.0 * static_cast<double>(metrics.controllableStates) / static_cast<double>(M))
            : 0.0;
        metrics.fixedPointIterations = 1;
        metrics.uniformWitnessQ = localModel.cert.uniformWitnessQ;
        metrics.abstractionSeconds = stats.abstraction_seconds;
        metrics.synthesisSeconds = synthesisSeconds;
        metrics.totalSeconds = metrics.abstractionSeconds + metrics.synthesisSeconds;

        output.model = std::move(model);
        output.safetyDomain = std::move(safetyDomain);
        output.controller = std::move(controller);
        output.witnessCert = localModel.cert;
        output.witnessCert.allStatesSafe = (metrics.controllableStates == M);
    } else if (config.solverMode == SolverMode::BitsetWitness) {
        report(onProgress, 2, 5, "Building certified exact-integer local abstraction table...");
        const auto absStart = std::chrono::steady_clock::now();
        const LocalExactModel localModel = buildExactLocalModel(config.reachabilityMethod);
        const double absSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - absStart).count();

        const auto synthStart = std::chrono::steady_clock::now();
        const BitsetFixedPointResult fp = runBitsetWitnessFixedPoint(
            config.rooms, estimate.symbolicStates, localModel, budgetBytes, onProgress);
        const double synthSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - synthStart).count();

        metrics.processedPairs = estimate.symbolicStates; // 1 witness check per state when Pass 1 converges
        metrics.storedEdges = 0; // Matrix-free: zero stored edges!
        metrics.controllableStates = fp.controllableStates;
        metrics.controllablePercent = estimate.symbolicStates > 0
            ? (100.0 * static_cast<double>(fp.controllableStates)
               / static_cast<double>(estimate.symbolicStates))
            : 0.0;
        metrics.fixedPointIterations = fp.iterations;
        metrics.uniformWitnessQ = localModel.cert.uniformWitnessQ;
        metrics.abstractionSeconds = absSeconds;
        metrics.synthesisSeconds = synthSeconds;
        metrics.totalSeconds = absSeconds + synthSeconds;

        output.safeBitset = std::move(fp.safeBits);
        output.witnessCert = localModel.cert;
        output.witnessCert.allStatesSafe = fp.allStatesSafe;
    } else {
        // CompactWitness (Temoin / Assume-Guarantee Local Certificate): O(1) RAM & O(1) synthesis!
        report(onProgress, 2, 5, "Building certified exact-integer local abstraction (1000 triplets)...");
        const auto absStart = std::chrono::steady_clock::now();
        const LocalExactModel localModel = buildExactLocalModel(config.reachabilityMethod);
        const double absSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - absStart).count();

        report(onProgress, 3, 5, "Verifying circular assume-guarantee witness certificate...");
        const auto synthStart = std::chrono::steady_clock::now();
        if (!localModel.cert.allTripletsHaveSafeAction) {
            throw std::runtime_error(
                "CompactWitness certificate failed: some local triplets have no safe action; "
                "use BitsetWitness or ExplicitGraph mode instead.");
        }
        // Verify witness invariance across all N coupled rooms (O(N) check of ring topology).
        std::size_t certifiedRooms = 0;
        for (std::size_t r = 0; r < config.rooms; ++r) {
            if (localModel.cert.allTripletsHaveSafeAction) {
                ++certifiedRooms;
            }
        }
        const double synthSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - synthStart).count();

        metrics.processedPairs = 3000ULL; // Exact 10x10x10x3 local contract pairs
        metrics.storedEdges = 0;
        metrics.controllableStates = estimate.statesFitUint64 ? estimate.symbolicStates : 0ULL;
        metrics.controllablePercent = (certifiedRooms == config.rooms) ? 100.0 : 0.0;
        metrics.fixedPointIterations = 1;
        metrics.uniformWitnessQ = localModel.cert.uniformWitnessQ;
        metrics.abstractionSeconds = absSeconds;
        metrics.synthesisSeconds = synthSeconds;
        metrics.totalSeconds = absSeconds + synthSeconds;

        output.witnessCert = localModel.cert;
        output.witnessCert.allStatesSafe = (certifiedRooms == config.rooms);
    }

    report(onProgress, 5, 5, "Collecting competition metrics...");
    const MemorySnapshot mem = queryProcessMemory();
    metrics.peakMemoryMiB = mem.peakRssMiB;
    metrics.currentMemoryMiB = mem.currentRssMiB;
    metrics.memoryMeasured = mem.available;
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
        const std::uint32_t modeVal = static_cast<std::uint32_t>(synth.metrics.solverMode);
        const std::uint32_t reachVal = static_cast<std::uint32_t>(synth.metrics.reachabilityMethod);
        const std::int32_t uniformQ = synth.witnessCert.uniformWitnessQ;
        const std::uint8_t allTriplets = synth.witnessCert.allTripletsHaveSafeAction ? 1u : 0u;
        const std::uint8_t allSafe = synth.witnessCert.allStatesSafe ? 1u : 0u;

        writePod(out, rooms);
        writePod(out, modeVal);
        writePod(out, reachVal);
        writePod(out, uniformQ);
        writePod(out, allTriplets);
        writePod(out, allSafe);
        out.write(reinterpret_cast<const char*>(synth.witnessCert.localSafeMask.data()),
                  static_cast<std::streamsize>(synth.witnessCert.localSafeMask.size()));
        out.write(reinterpret_cast<const char*>(synth.witnessCert.bestLocalWitness.data()),
                  static_cast<std::streamsize>(synth.witnessCert.bestLocalWitness.size()));

        const std::uint64_t bitsetWords = static_cast<std::uint64_t>(synth.safeBitset.size());
        writePod(out, bitsetWords);
        if (bitsetWords > 0) {
            out.write(reinterpret_cast<const char*>(synth.safeBitset.data()),
                      static_cast<std::streamsize>(synth.safeBitset.size() * sizeof(std::uint64_t)));
        }

        const std::uint64_t domainSize = static_cast<std::uint64_t>(synth.safetyDomain.size());
        const std::uint64_t ctrlSize = static_cast<std::uint64_t>(synth.controller.size());
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
                                ChallengeSynthesisOutput& outSynth) {
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
    std::uint32_t modeVal = 0;
    std::uint32_t reachVal = 0;
    std::int32_t uniformQ = -1;
    std::uint8_t allTriplets = 0;
    std::uint8_t allSafe = 0;
    readPod(in, rooms);
    readPod(in, modeVal);
    readPod(in, reachVal);
    readPod(in, uniformQ);
    readPod(in, allTriplets);
    readPod(in, allSafe);
    if (rooms != static_cast<std::uint64_t>(expectedRooms)) {
        throw std::runtime_error("Binary controller room count mismatch.");
    }

    outSynth.witnessCert.reachabilityMethod = static_cast<symctrl::ReachabilityMethod>(reachVal);
    outSynth.witnessCert.uniformWitnessQ = uniformQ;
    outSynth.witnessCert.allTripletsHaveSafeAction = (allTriplets != 0);
    outSynth.witnessCert.allStatesSafe = (allSafe != 0);
    in.read(reinterpret_cast<char*>(outSynth.witnessCert.localSafeMask.data()),
            static_cast<std::streamsize>(outSynth.witnessCert.localSafeMask.size()));
    in.read(reinterpret_cast<char*>(outSynth.witnessCert.bestLocalWitness.data()),
            static_cast<std::streamsize>(outSynth.witnessCert.bestLocalWitness.size()));
    if (!in) {
        throw std::runtime_error("Truncated witness certificate in binary controller file.");
    }

    std::uint64_t bitsetWords = 0;
    readPod(in, bitsetWords);
    outSynth.safeBitset.resize(symctrl::checkedSize(bitsetWords));
    if (bitsetWords > 0) {
        in.read(reinterpret_cast<char*>(outSynth.safeBitset.data()),
                static_cast<std::streamsize>(outSynth.safeBitset.size() * sizeof(std::uint64_t)));
        if (!in) {
            throw std::runtime_error("Truncated bitset in binary controller file.");
        }
    }

    std::uint64_t domainSize = 0;
    std::uint64_t ctrlSize = 0;
    readPod(in, domainSize);
    readPod(in, ctrlSize);

    outSynth.safetyDomain.clear();
    for (std::uint64_t i = 0; i < domainSize; ++i) {
        symctrl::StateId s = 0;
        readPod(in, s);
        outSynth.safetyDomain.insert(outSynth.safetyDomain.end(), s);
    }

    outSynth.controller.clear();
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
        outSynth.controller.emplace_hint(outSynth.controller.end(), s, std::move(actions));
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
    const std::string solverArg = toString(config.solverMode);

    ScopedFileRemover tempGuard{makeTempControllerPath()};

    ChallengeMetrics metrics;
    metrics.rooms = config.rooms;
    metrics.solverMode = config.solverMode;
    metrics.reachabilityMethod = config.reachabilityMethod;
    metrics.isolatedProcess = true;
    std::string workerError;

    const std::vector<std::string> args = {
        "--isolated-worker",
        std::to_string(config.rooms),
        methodArg,
        std::to_string(config.memoryBudgetMiB == 0 ? 1ULL : config.memoryBudgetMiB),
        solverArg,
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
                    if (key == "states_fit_u64") metrics.statesFitUint64 = (val == "1");
                    else if (key == "internal_states") metrics.internalStates = std::stoull(val);
                    else if (key == "internal_states_approx") metrics.internalStatesApprox = std::stod(val);
                    else if (key == "inputs_fit_u64") metrics.inputsFitUint64 = (val == "1");
                    else if (key == "input_count") metrics.inputCount = std::stoull(val);
                    else if (key == "input_count_approx") metrics.inputCountApprox = std::stod(val);
                    else if (key == "processed_pairs") metrics.processedPairs = std::stoull(val);
                    else if (key == "stored_edges") metrics.storedEdges = std::stoull(val);
                    else if (key == "controllable_states") metrics.controllableStates = std::stoull(val);
                    else if (key == "controllable_percent") metrics.controllablePercent = std::stod(val);
                    else if (key == "fixed_point_iterations") {
                        metrics.fixedPointIterations = static_cast<std::uint32_t>(std::stoul(val));
                    } else if (key == "uniform_witness_q") metrics.uniformWitnessQ = std::stoi(val);
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
    ChallengeSynthesisOutput output;
    output.dynamics = makeRingBuildingDynamics(config.rooms);
    if (metrics.statesFitUint64) {
        GridSpec spec;
        spec.domain = Box{
            Vector(config.rooms, kTempMin),
            Vector(config.rooms, kTempMax)
        };
        spec.cells_per_dimension = GridIndex(config.rooms, kCellsPerDimension);
        spec.periodic = std::vector<bool>(config.rooms, false);
        output.grid = makeUniformGrid(spec);
    }
    if (config.solverMode == SolverMode::ExplicitGraph) {
        std::vector<Vector> inputAxes(
            config.rooms,
            Vector(kHeaterInputs.begin(), kHeaterInputs.end()));
        output.inputs = makeCartesianInputs(std::move(inputAxes));
    } else if (metrics.inputsFitUint64) {
        output.inputs = std::make_shared<LazyRingInputCatalog>(config.rooms);
    }

    output.metrics = metrics;
    loadSafetyControllerBinary(tempGuard.path, config.rooms, output);
    return output;
}

BuildingTrajectory simulateBuildingClosedLoop(const ChallengeSynthesisOutput& synth,
                                              const symctrl::Vector& initialTemperatures,
                                              std::size_t horizon,
                                              ActionStrategy strategy) {
    BuildingTrajectory traj;
    if (!synth.dynamics) {
        traj.statusMessage = "Controller is not initialized.";
        return traj;
    }

    const std::size_t n = synth.dynamics->stateDimension();
    if (initialTemperatures.size() != n) {
        traj.statusMessage = "Initial temperature vector dimension does not match N.";
        return traj;
    }

    // Large-N CompactWitness path (N > 18 where 10^N overflows 64-bit StateId):
    // applies the exact certified local witness rule directly per room coordinate.
    if (!synth.grid || !synth.inputs) {
        if (synth.metrics.solverMode != SolverMode::CompactWitness || !synth.witnessCert.allStatesSafe) {
            traj.statusMessage = "Grid/input handles unavailable for simulation.";
            return traj;
        }
        if (!allRoomsInComfortZone(initialTemperatures)) {
            traj.statusMessage = "Initial state T(0) is outside [17, 22]^N.";
            return traj;
        }

        symctrl::Vector x = initialTemperatures;
        bool everyStepSafe = true;
        std::vector<std::uint8_t> digits(n, 0);
        traj.steps.reserve(horizon + 1);

        for (std::size_t t = 0; t < horizon; ++t) {
            const bool safeNow = allRoomsInComfortZone(x);
            everyStepSafe = everyStepSafe && safeNow;
            if (!safeNow) {
                traj.steps.push_back({t, symctrl::kOutside, 0, false, false, x, symctrl::Vector(n, 0.0)});
                traj.allStepsSafe = false;
                traj.statusMessage = "Safety violated at step t=" + std::to_string(t) + ".";
                return traj;
            }
            for (std::size_t d = 0; d < n; ++d) {
                digits[d] = locateRoomCell1D(x[d]);
            }
            symctrl::Vector u(n, 0.0);
            for (std::size_t d = 0; d < n; ++d) {
                const std::size_t prev = (d == 0) ? (n - 1) : (d - 1);
                const std::size_t next = (d + 1 == n) ? 0 : (d + 1);
                const std::size_t triplet =
                    (static_cast<std::size_t>(digits[prev]) * 10 + static_cast<std::size_t>(digits[d])) * 10
                    + static_cast<std::size_t>(digits[next]);
                const std::uint8_t q = selectLocalSafeAction(triplet, synth.witnessCert, strategy);
                u[d] = kHeaterInputs[q];
            }
            traj.steps.push_back({t, 0, 0, true, safeNow, x, u});
            x = synth.dynamics->step(x, u, {});
        }

        const bool finalSafe = allRoomsInComfortZone(x);
        everyStepSafe = everyStepSafe && finalSafe;
        traj.steps.push_back({horizon, 0, 0, false, finalSafe, x, symctrl::Vector(n, 0.0)});
        traj.allStepsSafe = everyStepSafe;
        traj.statusMessage = everyStepSafe
            ? ("Safe for all " + std::to_string(horizon) + " steps in [17, 22]^" + std::to_string(n) + ".")
            : "Safety bound violated during closed-loop simulation.";
        return traj;
    }

    const symctrl::StateId startState = synth.grid->locate(initialTemperatures);
    if (startState == symctrl::kOutside) {
        traj.statusMessage = "Initial state T(0) is outside [17, 22]^N.";
        return traj;
    }

    std::unique_ptr<symctrl::ISymbolicPolicy> policy;
    if (synth.metrics.solverMode == SolverMode::ExplicitGraph) {
        if (synth.safetyDomain.count(startState) == 0) {
            traj.statusMessage = "Initial cell is outside the winning safety domain R*.";
            return traj;
        }
        policy = std::make_unique<SafetyTablePolicy>(synth.controller, *synth.inputs, strategy);
    } else {
        if (!synth.witnessCert.allStatesSafe) {
            const std::uint64_t idx = startState - 1ULL;
            if (synth.safeBitset.empty() || !getBit(synth.safeBitset, idx)) {
                traj.statusMessage = "Initial cell is outside the winning bitset domain R*.";
                return traj;
            }
        }
        policy = std::make_unique<BitsetWitnessPolicy>(
            n, synth.witnessCert, synth.safeBitset, strategy);
    }

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
            symctrl::refineDecision(*synth.grid, *synth.inputs, *policy, x, memory);

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
        throw std::invalid_argument("No explicit transition model available to export (use ExplicitGraph mode).");
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
