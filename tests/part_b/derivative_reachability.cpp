#include "test.hpp"
#include "symctrl/contracts/factories.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

using namespace symctrl;

namespace {
class TestDynamics final : public IDynamics {
public:
    using Step = std::function<Vector(const Vector&, const Vector&, const Vector&)>;
    using Bounds = std::function<DerivativeBounds(const Box&, const Vector&, const Box&)>;

private:
    const std::size_t n_, m_, q_;
    const Step step_;
    const Bounds bounds_;

public:
    TestDynamics(std::size_t n, std::size_t m, std::size_t q, Step step, Bounds bounds)
        : n_(n), m_(m), q_(q), step_(std::move(step)), bounds_(std::move(bounds)) {}
    std::size_t stateDimension() const noexcept override { return n_; }
    std::size_t inputDimension() const noexcept override { return m_; }
    std::size_t disturbanceDimension() const noexcept override { return q_; }
    Vector step(const Vector& x, const Vector& u, const Vector& w) const override {
        return step_(x, u, w);
    }
    bool cooperativeOn(const Box&, const Vector&, const Box&) const override {
        throw UnsupportedCapability("derivative test model supplies no monotone capability");
    }
    DerivativeBounds absoluteJacobianBounds(const Box& cell, const Vector& input,
                                           const Box& disturbances) const override {
        return bounds_(cell, input, disturbances);
    }
};

std::shared_ptr<const IDynamics> mixedDynamics() {
    return std::make_shared<TestDynamics>(2, 1, 2,
        [](const Vector& x, const Vector& u, const Vector& w) -> Vector {
            return {2*x[0] - x[1] + u[0] - w[0] + 2*w[1],
                    -3*x[0] + 0.5*x[1] - 2*u[0] + w[0] - 2*w[1]};
        },
        [](const Box&, const Vector&, const Box&) -> DerivativeBounds {
            return {{{2,1}, {3,0.5}}, {{1,2}, {1,2}}};
        });
}

std::shared_ptr<const IReachability> backend(std::shared_ptr<const IDynamics> dynamics) {
    return makeReachability(std::move(dynamics), ReachabilityMethod::BoundedDerivative);
}

void equalBox(const Box& actual, const Box& expected) {
    CHECK(actual.lo.size() == expected.lo.size());
    CHECK(actual.hi.size() == expected.hi.size());
    for (std::size_t d = 0; d < expected.lo.size(); ++d) {
        CHECK(test::near(actual.lo[d], expected.lo[d]));
        CHECK(test::near(actual.hi[d], expected.hi[d]));
    }
}

void affine() {
    const auto reach = backend(mixedDynamics());
    CHECK(reach->stateDimension() == 2 && reach->inputDimension() == 1);
    CHECK(reach->disturbanceDimension() == 2);
    CHECK(reach->numericGuarantee() == NumericGuarantee::UncertifiedFloat);
    const Box cell{{1,2}, {2,4}}, disturbances{{-1,0}, {1,2}};
    equalBox(reach->enclose(cell, {0}, disturbances), {{-3,-10}, {7,0}});
    equalBox(reach->enclose(cell, {2}, disturbances), {{-1,-14}, {9,-4}});
    equalBox(reach->enclose({{1,2},{1,2}}, {0}, {{-1,0},{-1,0}}), {{1,-3},{1,-3}});
}

void noDisturbance() {
    auto dynamics = std::make_shared<TestDynamics>(3, 2, 0,
        [](const Vector& x, const Vector& u, const Vector& w) -> Vector {
            CHECK(w.empty());
            return {-2*x[0] + u[0], x[1] + x[2] - u[1], -x[2]};
        },
        [](const Box&, const Vector&, const Box& disturbances) -> DerivativeBounds {
            CHECK(disturbances.lo.empty() && disturbances.hi.empty());
            return {{{2,0,0}, {0,1,1}, {0,0,1}}, {{},{},{}}};
        });
    const auto reach = backend(dynamics);
    CHECK(reach->stateDimension() == 3 && reach->inputDimension() == 2);
    CHECK(reach->disturbanceDimension() == 0);
    equalBox(reach->enclose({{1,2,3},{2,4,5}}, {3,1}, {{},{}}), {{-1,4,-5},{1,8,-3}});
}

void fullBox() {
    std::size_t boundCalls = 0, stepCalls = 0;
    Box seenCell, seenNoise;
    Vector seenInput, seenCentre, seenNoiseCentre;
    auto dynamics = std::make_shared<TestDynamics>(1, 1, 1,
        [&](const Vector& x, const Vector& u, const Vector& w) -> Vector {
            ++stepCalls;
            seenCentre = x;
            seenNoiseCentre = w;
            return {x[0]*x[0] + u[0] + w[0]*w[0]};
        },
        [&](const Box& cell, const Vector& input, const Box& disturbances) -> DerivativeBounds {
            ++boundCalls;
            seenCell = cell;
            seenInput = input;
            seenNoise = disturbances;
            // |d(x^2)/dx| <= 2*max(|lo|,|hi|) on the whole interval.
            return {{{2*std::max(std::abs(cell.lo[0]), std::abs(cell.hi[0]))}},
                    {{2*std::max(std::abs(disturbances.lo[0]), std::abs(disturbances.hi[0]))}}};
        });
    const auto reach = backend(dynamics);
    CHECK(boundCalls == 0 && stepCalls == 0);
    const Box cell{{-2},{3}}, disturbances{{-1},{2}};
    equalBox(reach->enclose(cell, {4}, disturbances), {{-16.5},{25.5}});
    CHECK(seenCell.lo == cell.lo && seenCell.hi == cell.hi);
    CHECK(seenNoise.lo == disturbances.lo && seenNoise.hi == disturbances.hi);
    CHECK(seenInput == Vector({4}));
    CHECK(seenCentre == Vector({0.5}) && seenNoiseCentre == Vector({0.5}));
    equalBox(reach->enclose({{0},{1}}, {1}, {{0},{0}}), {{0.25},{2.25}});
    CHECK(boundCalls == 2 && stepCalls == 2);
}

void invalidRequests() {
    std::size_t callbacks = 0;
    auto dynamics = std::make_shared<TestDynamics>(2, 1, 1,
        [&](const Vector&, const Vector&, const Vector&) -> Vector { ++callbacks; return {0,0}; },
        [&](const Box&, const Vector&, const Box&) -> DerivativeBounds {
            ++callbacks;
            return {{{1,0},{0,1}}, {{0},{0}}};
        });
    const auto reach = backend(dynamics);
    const Box cell{{0,0},{1,1}}, disturbances{{0},{1}};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    struct Request { Box cell; Vector input; Box disturbances; };
    const std::vector<Request> invalid{
        {{{},{}}, {0}, disturbances}, {{{0},{1}}, {0}, disturbances},
        {{{0,0},{1}}, {0}, disturbances}, {{{1,0},{0,1}}, {0}, disturbances},
        {{{nan,0},{1,1}}, {0}, disturbances}, {{{0,0},{1,inf}}, {0}, disturbances},
        {cell, {}, disturbances}, {cell, {0,1}, disturbances}, {cell, {nan}, disturbances},
        {cell, {0}, {{},{}}}, {cell, {0}, {{0,0},{1,1}}},
        {cell, {0}, {{1},{0}}}, {cell, {0}, {{0},{inf}}}, {cell, {0}, {{nan},{1}}}
    };
    for (const auto& request : invalid)
        test::throws<std::invalid_argument>([&] { reach->enclose(request.cell, request.input, request.disturbances); });
    CHECK(callbacks == 0);
}

void invalidBounds() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<DerivativeBounds> invalid{
        {{}, {{0},{0}}}, {{{1},{0}}, {{0},{0}}}, {{{1,0},{0,1}}, {}},
        {{{1,0},{0,1}}, {{0,1},{0}}}, {{{-1,0},{0,1}}, {{0},{0}}},
        {{{1,0},{0,1}}, {{-1},{0}}}, {{{inf,0},{0,1}}, {{0},{0}}},
        {{{1,0},{0,1}}, {{0},{nan}}}
    };
    for (const auto& bounds : invalid) {
        std::size_t steps = 0;
        auto dynamics = std::make_shared<TestDynamics>(2, 1, 1,
            [&](const Vector&, const Vector&, const Vector&) -> Vector { ++steps; return {0,0}; },
            [bounds](const Box&, const Vector&, const Box&) { return bounds; });
        test::throws<std::invalid_argument>([&] { backend(dynamics)->enclose({{0,0},{1,1}}, {0}, {{0},{1}}); });
        CHECK(steps == 0);
    }
    auto missingEmptyRows = std::make_shared<TestDynamics>(1, 1, 0,
        [](const Vector& x, const Vector&, const Vector&) { return x; },
        [](const Box&, const Vector&, const Box&) -> DerivativeBounds { return {{{1}}, {}}; });
    test::throws<std::invalid_argument>([&] { backend(missingEmptyRows)->enclose({{0},{1}}, {0}, {{},{}}); });
}

void invalidOutputs() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const auto& output : std::vector<Vector>{{}, {0}, {0,0,0}, {nan,0}, {0,inf}}) {
        auto dynamics = std::make_shared<TestDynamics>(2, 1, 0,
            [output](const Vector&, const Vector&, const Vector&) { return output; },
            [](const Box&, const Vector&, const Box&) -> DerivativeBounds {
                return {{{1,0},{0,1}}, {{},{}}};
            });
        test::throws<std::invalid_argument>([&] { backend(dynamics)->enclose({{0,0},{1,1}}, {0}, {{},{}}); });
    }
}

void factory() {
    test::throws<std::invalid_argument>([] { backend(nullptr); });
    for (const auto dimensions : {std::pair<std::size_t,std::size_t>{0,1}, {1,0}}) {
        auto dynamics = std::make_shared<TestDynamics>(dimensions.first, dimensions.second, 0,
            [](const Vector&, const Vector&, const Vector&) -> Vector { return {}; },
            [](const Box&, const Vector&, const Box&) -> DerivativeBounds { return {}; });
        test::throws<std::invalid_argument>([&] { backend(dynamics); });
    }
    test::throws<UnsupportedCapability>([] {
        makeReachability(mixedDynamics(), ReachabilityMethod::BoundedDerivative, {true});
    });
}

void lifetime() {
    std::weak_ptr<const IDynamics> weak;
    std::shared_ptr<const IReachability> reach;
    {
        auto dynamics = mixedDynamics();
        weak = dynamics;
        reach = backend(dynamics);
    }
    CHECK(!weak.expired());
    auto first = reach->enclose({{1,2},{2,4}}, {0}, {{-1,0},{1,2}});
    first.lo[0] = -100;
    equalBox(reach->enclose({{1,2},{2,4}}, {0}, {{-1,0},{1,2}}), {{-3,-10},{7,0}});
    reach.reset();
    CHECK(weak.expired() && first.hi == Vector({7,0}));
}

void extremes() {
    auto identity = std::make_shared<TestDynamics>(1, 1, 0,
        [](const Vector& x, const Vector&, const Vector&) { return x; },
        [](const Box&, const Vector&, const Box&) -> DerivativeBounds { return {{{1}}, {{}}}; });
    const auto reach = backend(identity);
    const double max = std::numeric_limits<double>::max();
    const auto wide = reach->enclose({{-max},{max}}, {0}, {{},{}});
    CHECK(wide.lo == Vector({-max}) && wide.hi == Vector({max}));
    const auto positive = reach->enclose({{max/4},{3*(max/4)}}, {0}, {{},{}});
    CHECK(std::isfinite(positive.lo[0]) && std::isfinite(positive.hi[0]));
    CHECK(positive.lo[0] <= max/4 && positive.hi[0] >= 3*(max/4));
    const double tiny = std::numeric_limits<double>::denorm_min();
    const auto narrow = reach->enclose({{0},{tiny}}, {0}, {{},{}});
    CHECK(narrow.lo[0] <= 0 && narrow.hi[0] >= tiny);
}

void overflow() {
    const double max = std::numeric_limits<double>::max();
    auto productOverflow = std::make_shared<TestDynamics>(1, 1, 0,
        [](const Vector&, const Vector&, const Vector&) -> Vector { return {0}; },
        [max](const Box&, const Vector&, const Box&) -> DerivativeBounds { return {{{max}}, {{}}}; });
    test::throws<std::overflow_error>([&] { backend(productOverflow)->enclose({{-2},{2}}, {0}, {{},{}}); });

    auto sumOverflow = std::make_shared<TestDynamics>(2, 1, 0,
        [](const Vector&, const Vector&, const Vector&) -> Vector { return {0,0}; },
        [max](const Box&, const Vector&, const Box&) -> DerivativeBounds {
            return {{{max,max},{0,0}}, {{},{}}};
        });
    test::throws<std::overflow_error>([&] { backend(sumOverflow)->enclose({{-1,-1},{1,1}}, {0}, {{},{}}); });

    auto endpointOverflow = std::make_shared<TestDynamics>(1, 1, 0,
        [max](const Vector&, const Vector&, const Vector&) -> Vector { return {max}; },
        [](const Box&, const Vector&, const Box&) -> DerivativeBounds { return {{{1}}, {{}}}; });
    test::throws<std::overflow_error>([&] { backend(endpointOverflow)->enclose({{-max},{max}}, {0}, {{},{}}); });
}

void exceptions() {
    auto unsupported = std::make_shared<TestDynamics>(1, 1, 0,
        [](const Vector& x, const Vector&, const Vector&) { return x; },
        [](const Box&, const Vector&, const Box&) -> DerivativeBounds {
            throw UnsupportedCapability("model supplies no derivative bounds");
        });
    test::throws<UnsupportedCapability>([&] { backend(unsupported)->enclose({{0},{1}}, {0}, {{},{}}); });
    auto throwingStep = std::make_shared<TestDynamics>(1, 1, 0,
        [](const Vector&, const Vector&, const Vector&) -> Vector { throw std::overflow_error("model overflow"); },
        [](const Box&, const Vector&, const Box&) -> DerivativeBounds { return {{{1}}, {{}}}; });
    test::throws<std::overflow_error>([&] { backend(throwingStep)->enclose({{0},{1}}, {0}, {{},{}}); });
}
}

int main(int argc, char** argv) {
    return test::run(argc, argv, {{"affine", affine}, {"no_disturbance", noDisturbance}, {"full_box", fullBox},
        {"invalid_requests", invalidRequests}, {"invalid_bounds", invalidBounds}, {"invalid_outputs", invalidOutputs},
        {"factory", factory}, {"lifetime", lifetime}, {"extremes", extremes}, {"overflow", overflow},
        {"exceptions", exceptions}});
}
