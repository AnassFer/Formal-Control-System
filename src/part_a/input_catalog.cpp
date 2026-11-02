#include "symctrl/contracts/factories.hpp"
#include <cmath>
#include <stdexcept>

namespace symctrl {
namespace {

// ---------------------------------------------------------------------------
// ExplicitInputCatalog — flat ordered list of input vectors.
//
// Caller-supplied order is preserved verbatim; duplicate physical vectors
// are legal and become distinct action IDs.  Stable const references are
// guaranteed for the catalogue lifetime (vectors stored by value).
// ---------------------------------------------------------------------------

class ExplicitInputCatalog final : public IInputCatalog {
public:
    explicit ExplicitInputCatalog(std::vector<Vector> values)
        : values_(std::move(values)) {
        if (values_.empty())
            throw std::invalid_argument("makeInputCatalog: empty input list");
        const std::size_t m = values_[0].size();
        if (m == 0)
            throw std::invalid_argument("makeInputCatalog: zero-dimensional input vector");
        // validateVector checks: correct size + all finite.
        for (const auto& v : values_)
            validateVector(v, m);
        m_ = m;
    }

    std::size_t inputDimension() const noexcept override { return m_; }
    InputId     inputCount()     const noexcept override {
        return static_cast<InputId>(values_.size());
    }

    const Vector& value(InputId input) const override {
        if (input >= static_cast<InputId>(values_.size()))
            throw std::out_of_range("value: input ID out of range");
        return values_[checkedSize(input)];
    }

private:
    std::vector<Vector> values_;
    std::size_t         m_{};
};

// ---------------------------------------------------------------------------
// CartesianInputCatalog — Cartesian product of per-axis value sets.
//
// `per_dimension_values[d]` lists the allowed physical values for coordinate d
// of the input vector.  Dimension-0 varies fastest (same odometer convention
// as IStateGrid).  All vectors are pre-built so that value() can return stable
// const references for the catalogue lifetime without on-the-fly allocation.
//
// Decoding:   for id i, coord d = axes_[d][ (i / stride[d]) % axes_[d].size() ]
// which simplifies to the iterated-modulo below.
// ---------------------------------------------------------------------------

class CartesianInputCatalog final : public IInputCatalog {
public:
    explicit CartesianInputCatalog(std::vector<Vector> per_dimension_values)
        : axes_(std::move(per_dimension_values)) {
        if (axes_.empty())
            throw std::invalid_argument("makeCartesianInputs: no axes supplied");

        m_ = axes_.size(); // each input vector has one coordinate per axis

        // Validate axes and compute total count.
        uint64_t count = 1;
        for (std::size_t d = 0; d < m_; ++d) {
            if (axes_[d].empty())
                throw std::invalid_argument("makeCartesianInputs: empty axis");
            for (double v : axes_[d])
                if (!std::isfinite(v))
                    throw std::invalid_argument("makeCartesianInputs: nonfinite value");
            count = checkedMultiply(count, static_cast<uint64_t>(axes_[d].size()));
        }
        (void)checkedSize(count); // ensure count fits in size_t
        count_ = count;

        // Pre-build every input vector using iterated-modulo (dimension-0 fastest).
        // For id i:  coord d  =  axes_[d][ remainder % axes_[d].size() ]
        //            then remainder /= axes_[d].size()
        vectors_.resize(checkedSize(count_));
        for (uint64_t id = 0; id < count_; ++id) {
            Vector v(m_);
            uint64_t rem = id;
            for (std::size_t d = 0; d < m_; ++d) {
                const uint64_t axis_size = static_cast<uint64_t>(axes_[d].size());
                v[d] = axes_[d][static_cast<std::size_t>(rem % axis_size)];
                rem /= axis_size;
            }
            vectors_[checkedSize(id)] = std::move(v);
        }
    }

    std::size_t inputDimension() const noexcept override { return m_; }
    InputId     inputCount()     const noexcept override { return count_; }

    const Vector& value(InputId input) const override {
        if (input >= count_)
            throw std::out_of_range("value: input ID out of range");
        return vectors_[checkedSize(input)];
    }

private:
    std::vector<Vector> axes_;    // kept for potential introspection
    std::vector<Vector> vectors_; // pre-built; stable refs returned by value()
    std::size_t         m_{};
    uint64_t            count_{};
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public factories — single definition per symbol in this translation unit.
// ---------------------------------------------------------------------------

std::shared_ptr<const IInputCatalog> makeInputCatalog(std::vector<Vector> values) {
    return std::make_shared<ExplicitInputCatalog>(std::move(values));
}

std::shared_ptr<const IInputCatalog> makeCartesianInputs(std::vector<Vector> per_dimension_values) {
    return std::make_shared<CartesianInputCatalog>(std::move(per_dimension_values));
}

} // namespace symctrl
