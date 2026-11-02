#include "symctrl/contracts/factories.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace symctrl {
namespace {

// ---------------------------------------------------------------------------
// UniformGrid — private concrete implementation of IStateGrid.
//
// Encoding (dimension-0 fastest):
//   stride[0] = 1,  stride[d] = prod(count[j], j < d)
//   id = 1 + sum(index[d] * stride[d])
//
// Point ownership (locate):
//   Each dimension uses half-open [lo, lo+k*eta) for k=0..count-2 and
//   closed [lo+(count-1)*eta, hi] for the last interval.
//   Internal faces go to the cell with the higher index.
//   Any coordinate strictly outside [domain.lo, domain.hi] → kOutside=0.
//
// Cell geometry (cellBounds):
//   Always returns a closed Box regardless of ownership convention.
//
// Intersection (intersectingInternalCells):
//   Conservative candidate bounds from source-map formula, then
//   enumerate all cells in the resulting n-dimensional range.
// ---------------------------------------------------------------------------

class UniformGrid final : public IStateGrid {
public:
    explicit UniformGrid(GridSpec spec) : spec_(std::move(spec)) {
        // validateGridSpec checks: positive widths, nonzero counts,
        // dimension consistency, and that M+1 fits in size_t.
        M_ = validateGridSpec(spec_);

        const std::size_t n = spec_.domain.lo.size();
        strides_.resize(n);
        eta_.resize(n);

        uint64_t stride = 1;
        for (std::size_t d = 0; d < n; ++d) {
            strides_[d] = stride;
            // checkedMultiply guards against overflow (already verified by
            // validateGridSpec, but kept explicit for clarity).
            stride = checkedMultiply(stride,
                static_cast<uint64_t>(spec_.cells_per_dimension[d]));
            eta_[d] = (spec_.domain.hi[d] - spec_.domain.lo[d])
                      / static_cast<double>(spec_.cells_per_dimension[d]);
        }
    }

    // -----------------------------------------------------------------------
    // Metadata
    // -----------------------------------------------------------------------

    std::size_t stateDimension() const noexcept override {
        return spec_.domain.lo.size();
    }

    const GridSpec& spec() const noexcept override { return spec_; }

    StateId internalStateCount() const noexcept override { return M_; }

    // -----------------------------------------------------------------------
    // locate: point → cell ID (or kOutside)
    // -----------------------------------------------------------------------

    StateId locate(const Vector& x) const override {
        const std::size_t n = stateDimension();
        validateVector(x, n); // throws invalid_argument on wrong size / NaN/Inf

        GridIndex idx(n);
        for (std::size_t d = 0; d < n; ++d) {
            const double lo = spec_.domain.lo[d];
            const double hi = spec_.domain.hi[d];
            // Strict check: any coord outside the closed domain → OUT.
            if (x[d] < lo || x[d] > hi) return kOutside;

            const double raw = (x[d] - lo) / eta_[d];
            uint64_t i = static_cast<uint64_t>(std::floor(raw));
            const uint64_t count = spec_.cells_per_dimension[d];
            // Clamp: x[d] exactly at domain.hi gives raw == count; keep it
            // in the last cell (outer boundary is part of the domain).
            if (i >= count) i = count - 1u;
            idx[d] = static_cast<uint32_t>(i);
        }
        return fromIndices(idx);
    }

    // -----------------------------------------------------------------------
    // cellBounds: cell ID → closed Box
    // -----------------------------------------------------------------------

    Box cellBounds(StateId state) const override {
        if (state == kOutside || state > M_)
            throw std::out_of_range("cellBounds: OUT has no finite cell box; valid range is 1..M");

        const std::size_t n = stateDimension();
        const GridIndex idx = indices(state); // already validated above

        Box b;
        b.lo.resize(n);
        b.hi.resize(n);
        for (std::size_t d = 0; d < n; ++d) {
            b.lo[d] = spec_.domain.lo[d] + static_cast<double>(idx[d])      * eta_[d];
            b.hi[d] = spec_.domain.lo[d] + static_cast<double>(idx[d] + 1u) * eta_[d];
        }
        return b;
    }

    // -----------------------------------------------------------------------
    // indices: cell ID → n-dimensional GridIndex
    // -----------------------------------------------------------------------

    GridIndex indices(StateId state) const override {
        if (state == kOutside || state > M_)
            throw std::out_of_range("indices: state ID out of range 1..M");

        const std::size_t n = stateDimension();
        GridIndex idx(n);
        uint64_t rem = state - 1u; // 0-based offset
        for (std::size_t d = 0; d < n; ++d) {
            const uint64_t count = spec_.cells_per_dimension[d];
            idx[d] = static_cast<uint32_t>(rem % count);
            rem /= count;
        }
        // rem == 0 here because state <= M = product of all counts.
        return idx;
    }

    // -----------------------------------------------------------------------
    // fromIndices: n-dimensional GridIndex → cell ID
    //   id = 1 + sum(index[d] * stride[d])
    // -----------------------------------------------------------------------

    StateId fromIndices(const GridIndex& index) const override {
        const std::size_t n = stateDimension();
        if (index.size() != n)
            throw std::invalid_argument("fromIndices: wrong number of indices");

        uint64_t id = 1u;
        for (std::size_t d = 0; d < n; ++d) {
            if (static_cast<uint64_t>(index[d]) >= spec_.cells_per_dimension[d])
                throw std::out_of_range("fromIndices: index component out of range");
            // index[d] * strides_[d] < M (proven by strides construction),
            // so checkedMultiply/Add are safe, but kept for correctness contract.
            id = checkedAdd(id,
                checkedMultiply(static_cast<uint64_t>(index[d]), strides_[d]));
        }
        return id;
    }

    // -----------------------------------------------------------------------
    // intersectingInternalCells: closed Box → sorted unique internal cell IDs
    //
    // Per dimension (source-map formula):
    //   first = max(0, ceil((Y.lo - L) / eta) - 1)   (conservative lower bound)
    //   last  = min(count-1, floor((Y.hi - L) / eta)) (conservative upper bound)
    // Empty range in any dimension → no intersecting cells.
    // -----------------------------------------------------------------------

    std::vector<StateId> intersectingInternalCells(const Box& closed_box) const override {
        const std::size_t n = stateDimension();
        validateBox(closed_box); // finiteness + lo<=hi + non-empty state box
        if (closed_box.lo.size() != n)
            throw std::invalid_argument("intersectingInternalCells: box dimension mismatch");

        // Compute per-dimension 0-based index ranges.
        std::vector<uint64_t> first(n), last(n);
        for (std::size_t d = 0; d < n; ++d) {
            const double L     = spec_.domain.lo[d];
            const uint64_t cnt = spec_.cells_per_dimension[d];

            // Conservative lower candidate (subtract 1 to handle boundary error).
            const double raw_lo = (closed_box.lo[d] - L) / eta_[d];
            const long long f_raw = static_cast<long long>(std::ceil(raw_lo)) - 1LL;
            first[d] = (f_raw < 0LL) ? 0u : static_cast<uint64_t>(f_raw);

            // Conservative upper candidate.
            const double raw_hi = (closed_box.hi[d] - L) / eta_[d];
            const long long l_raw = static_cast<long long>(std::floor(raw_hi));
            if (l_raw < 0LL) return {}; // box entirely below domain in this dim
            const uint64_t l_clamped = static_cast<uint64_t>(l_raw);
            last[d] = (l_clamped >= cnt) ? cnt - 1u : l_clamped;

            if (first[d] > last[d]) return {}; // no overlap in this dimension
        }

        // Enumerate all cells in the n-dimensional range [first..last] using
        // dimension-0-fastest order and collect their IDs.
        std::vector<StateId> result;
        GridIndex idx(n);
        for (std::size_t d = 0; d < n; ++d)
            idx[d] = static_cast<uint32_t>(first[d]);

        while (true) {
            result.push_back(fromIndices(idx));

            // Increment index in dimension-0-fastest order (odometer).
            std::size_t d = 0;
            while (d < n) {
                if (static_cast<uint64_t>(idx[d]) < last[d]) {
                    ++idx[d];
                    break;
                }
                idx[d] = static_cast<uint32_t>(first[d]);
                ++d;
            }
            if (d == n) break; // all dimensions wrapped: enumeration complete
        }

        // IDs from a rectangular scan in dimension-0-fastest order are already
        // sorted ascending (each increment of idx[0] gives id+1, and carrying
        // into higher dimensions gives larger strides). Sort anyway to be safe
        // in case stride ordering is non-obvious for edge cases.
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

private:
    GridSpec             spec_;
    StateId              M_{};               // total internal state count
    std::vector<uint64_t> strides_;          // stride[d] = prod(count[j], j<d)
    std::vector<double>   eta_;              // cell width per dimension
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public factory — the only symbol defined in this translation unit.
// ---------------------------------------------------------------------------

std::shared_ptr<const IStateGrid> makeUniformGrid(GridSpec spec) {
    return std::make_shared<UniformGrid>(std::move(spec));
}

} // namespace symctrl
