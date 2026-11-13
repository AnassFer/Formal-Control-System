#include "test.hpp"
#include "symctrl/contracts/factories.hpp"
#include <limits>
#include <memory>
#include <utility>

using namespace symctrl;

namespace {
class TestDynamics final : public IDynamics {
public:
    using Step = std::function<Vector(const Vector&, const Vector&, const Vector&)>;
    using Cooperative = std::function<bool(const Box&, const Vector&, const Box&)>;

private:
    const std::size_t n_, m_, q_;
    const Step step_;
    const Cooperative cooperative_;

public:
    TestDynamics(std::size_t n, std::size_t m, std::size_t q, Step step,
                 Cooperative cooperative = [](const Box&, const Vector&, const Box&) { return true; })
        : n_(n), m_(m), q_(q), step_(std::move(step)), cooperative_(std::move(cooperative)) {}

    std::size_t stateDimension() const noexcept override { return n_; }
    std::size_t inputDimension() const noexcept override { return m_; }
    std::size_t disturbanceDimension() const noexcept override { return q_; }
    Vector step(const Vector& x, const Vector& u, const Vector& w) const override {
        return step_(x, u, w);
    }
    bool cooperativeOn(const Box& cell, const Vector& input, const Box& disturbances) const override {
        return cooperative_(cell, input, disturbances);
    }
    DerivativeBounds absoluteJacobianBounds(const Box&, const Vector&, const Box&) const override {
        throw UnsupportedCapability("test model supplies no derivative bounds");
    }
};

std::shared_ptr<const IDynamics> positiveDynamics() {
    // Every state/disturbance coefficient is nonnegative; the fixed input may have either sign.
    return std::make_shared<TestDynamics>(2, 1, 2,
        [](const Vector& x, const Vector& u, const Vector& w) -> Vector {
            return {2*x[0] + x[1] + u[0] + w[0], 3*x[1] - 2*u[0] + 2*w[0] + w[1]};
        });
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
    const auto backend = makeReachability(positiveDynamics(), ReachabilityMethod::CooperativeMonotone);
    CHECK(backend->stateDimension() == 2);
    CHECK(backend->inputDimension() == 1);
    CHECK(backend->disturbanceDimension() == 2);
    CHECK(backend->numericGuarantee() == NumericGuarantee::UncertifiedFloat);
    const Box states{{1,2}, {2,4}}, disturbances{{-1,0}, {1,2}};
    equalBox(backend->enclose(states, {0}, disturbances), {{3,4}, {9,16}});
    equalBox(backend->enclose(states, {2}, disturbances), {{5,0}, {11,12}});
}

void noDisturbance() {
    auto dynamics = std::make_shared<TestDynamics>(3, 2, 0,
        [](const Vector& x, const Vector& u, const Vector& w) -> Vector {
            CHECK(w.empty());
            return {x[0] + u[0], 2*x[1] - u[1], x[0] + x[2]};
        });
    const auto backend = makeReachability(dynamics, ReachabilityMethod::CooperativeMonotone);
    CHECK(backend->stateDimension() == 3 && backend->inputDimension() == 2);
    CHECK(backend->disturbanceDimension() == 0);
    equalBox(backend->enclose({{1,2,3}, {2,4,5}}, {3,1}, {{},{}}), {{4,3,4}, {5,7,7}});
    test::throws<std::invalid_argument>([&] {
        backend->enclose({{1,2,3}, {2,4,5}}, {3,1}, {{0},{0}});
    });
}

void pointBoxes() {
    const auto backend = makeReachability(positiveDynamics(), ReachabilityMethod::CooperativeMonotone);
    equalBox(backend->enclose({{1,2}, {1,2}}, {2}, {{-1,0}, {-1,0}}), {{5,0}, {5,0}});
    equalBox(backend->enclose({{1,2}, {1,2}}, {2}, {{-1,0}, {1,2}}), {{5,0}, {7,6}});
}

void capabilities() {
    std::size_t checks = 0, steps = 0;
    Box seenCell, seenDisturbances;
    Vector seenInput;
    auto dynamics = std::make_shared<TestDynamics>(1, 1, 1,
        [&](const Vector& x, const Vector& u, const Vector& w) -> Vector {
            ++steps;
            return {u[0]*x[0]*x[0] + w[0]};
        },
        [&](const Box& cell, const Vector& input, const Box& disturbances) {
            ++checks;
            seenCell = cell;
            seenInput = input;
            seenDisturbances = disturbances;
            // Analytic derivative 2*u*x is nonnegative throughout these state boxes.
            return cell.lo[0] >= 0 && input[0] >= 0;
        });
    const auto backend = makeReachability(dynamics, ReachabilityMethod::CooperativeMonotone);
    CHECK(checks == 0 && steps == 0); // Capability needs a domain, unavailable at construction.
    const Box states{{1}, {3}}, disturbances{{-2}, {4}};
    equalBox(backend->enclose(states, {2}, disturbances), {{0}, {22}});
    CHECK(seenCell.lo == states.lo && seenCell.hi == states.hi);
    CHECK(seenInput == Vector({2}));
    CHECK(seenDisturbances.lo == disturbances.lo && seenDisturbances.hi == disturbances.hi);
    test::throws<UnsupportedCapability>([&] { backend->enclose({{-1},{3}}, {2}, disturbances); });
    test::throws<UnsupportedCapability>([&] { backend->enclose(states, {-2}, disturbances); });
    CHECK(checks == 3 && steps == 2);
    equalBox(backend->enclose({{0},{2}}, {1}, {{0},{0}}), {{0},{4}});
    CHECK(checks == 4 && steps == 4);
}

void invalidRequests() {
    std::size_t callbacks = 0;
    auto dynamics = std::make_shared<TestDynamics>(2, 1, 2,
        [&](const Vector&, const Vector&, const Vector&) -> Vector { ++callbacks; return {0,0}; },
        [&](const Box&, const Vector&, const Box&) { ++callbacks; return true; });
    const auto backend = makeReachability(dynamics, ReachabilityMethod::CooperativeMonotone);
    const Box states{{1,2}, {2,4}}, disturbances{{-1,0}, {1,2}};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    struct Request { Box cell; Vector input; Box disturbances; };
    const std::vector<Request> invalid{
        {{{},{}}, {0}, disturbances},
        {{{1},{2}}, {0}, disturbances},
        {{{1,2},{2}}, {0}, disturbances},
        {{{2,2},{1,4}}, {0}, disturbances},
        {{{nan,2},{2,4}}, {0}, disturbances},
        {{{1,2},{inf,4}}, {0}, disturbances},
        {states, {}, disturbances},
        {states, {0,1}, disturbances},
        {states, {nan}, disturbances},
        {states, {-inf}, disturbances},
        {states, {0}, {{},{}}},
        {states, {0}, {{-1},{1}}},
        {states, {0}, {{-1,0},{1}}},
        {states, {0}, {{2,0},{1,2}}},
        {states, {0}, {{-inf,0},{1,2}}},
        {states, {0}, {{-1,0},{1,nan}}}
    };
    for (const auto& request : invalid) {
        test::throws<std::invalid_argument>([&] {
            backend->enclose(request.cell, request.input, request.disturbances);
        });
    }
    CHECK(callbacks == 0);
}

void invalidOutputs() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<Box> invalid{
        {{},{1,1}}, {{0},{1,1}}, {{0,0},{1}}, {{0,0},{1,1,1}},
        {{nan,0},{1,1}}, {{0,0},{1,inf}}, {{-inf,0},{1,1}}, {{0,0},{1,nan}},
        {{2,0},{1,1}}
    };
    for (const auto& output : invalid) {
        auto dynamics = std::make_shared<TestDynamics>(2, 1, 0,
            [output](const Vector& x, const Vector&, const Vector&) {
                return x[0] == 0 ? output.lo : output.hi;
            });
        const auto backend = makeReachability(dynamics, ReachabilityMethod::CooperativeMonotone);
        test::throws<std::invalid_argument>([&] {
            backend->enclose({{0,0},{1,1}}, {0}, {{},{}});
        });
    }
}

void factory() {
    test::throws<std::invalid_argument>([] {
        makeReachability(nullptr, ReachabilityMethod::CooperativeMonotone);
    });
    for (const auto dimensions : {std::pair<std::size_t,std::size_t>{0,1}, {1,0}}) {
        auto dynamics = std::make_shared<TestDynamics>(dimensions.first, dimensions.second, 0,
            [](const Vector&, const Vector&, const Vector&) -> Vector { return {}; });
        test::throws<std::invalid_argument>([&] {
            makeReachability(dynamics, ReachabilityMethod::CooperativeMonotone);
        });
    }
    test::throws<std::invalid_argument>([] {
        makeReachability(positiveDynamics(), static_cast<ReachabilityMethod>(99));
    });
    test::throws<UnsupportedCapability>([] {
        makeReachability(positiveDynamics(), ReachabilityMethod::CooperativeMonotone, {true});
    });
}

void lifetime() {
    std::weak_ptr<const IDynamics> weak;
    std::shared_ptr<const IReachability> backend;
    {
        auto dynamics = positiveDynamics();
        weak = dynamics;
        backend = makeReachability(dynamics, ReachabilityMethod::CooperativeMonotone);
    }
    CHECK(!weak.expired());
    auto first = backend->enclose({{1,2},{2,4}}, {0}, {{-1,0},{1,2}});
    first.lo[0] = -100;
    equalBox(backend->enclose({{1,2},{2,4}}, {0}, {{-1,0},{1,2}}), {{3,4},{9,16}});
    backend.reset();
    CHECK(weak.expired());
    CHECK(first.hi == Vector({9,16})); // Returned bounds own their storage independently.
}

void exceptions() {
    auto capabilityFailure = std::make_shared<TestDynamics>(1, 1, 0,
        [](const Vector& x, const Vector&, const Vector&) { return x; },
        [](const Box&, const Vector&, const Box&) -> bool {
            throw UnsupportedCapability("model has no monotonicity justification");
        });
    auto backend = makeReachability(capabilityFailure, ReachabilityMethod::CooperativeMonotone);
    test::throws<UnsupportedCapability>([&] { backend->enclose({{0},{1}}, {0}, {{},{}}); });

    auto stepFailure = std::make_shared<TestDynamics>(1, 1, 0,
        [](const Vector&, const Vector&, const Vector&) -> Vector {
            throw std::overflow_error("model arithmetic overflow");
        });
    backend = makeReachability(stepFailure, ReachabilityMethod::CooperativeMonotone);
    test::throws<std::overflow_error>([&] { backend->enclose({{0},{1}}, {0}, {{},{}}); });
}
}

int main(int argc, char** argv) {
    return test::run(argc, argv, {{"affine", affine}, {"no_disturbance", noDisturbance},
        {"point_boxes", pointBoxes}, {"capabilities", capabilities}, {"invalid_requests", invalidRequests},
        {"invalid_outputs", invalidOutputs}, {"factory", factory}, {"lifetime", lifetime},
        {"exceptions", exceptions}});
}
