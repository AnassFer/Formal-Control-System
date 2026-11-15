#include "test.hpp"
#include "symctrl/contracts/factories.hpp"
#include <limits>

using namespace symctrl;

namespace {
void equations() {
    const auto thermostat = makeThermostatDynamics();
    CHECK(thermostat->stateDimension() == 1 && thermostat->inputDimension() == 1);
    CHECK(thermostat->disturbanceDimension() == 1);
    CHECK(test::near(thermostat->step({18}, {1}, {10})[0], 19.6));
    CHECK(test::near(thermostat->step({19.6}, {0}, {10})[0], 18.64));
    for (std::size_t rooms : {3u, 4u, 5u}) {
        const auto building = makeRingBuildingDynamics(rooms);
        CHECK(building->stateDimension() == rooms && building->inputDimension() == rooms);
        CHECK(building->disturbanceDimension() == 0);
        const auto off = building->step(Vector(rooms, 20), Vector(rooms, 0), {});
        const auto on = building->step(Vector(rooms, 20), Vector(rooms, 0.6), {});
        for (std::size_t i = 0; i < rooms; ++i) {
            CHECK(test::near(off[i], 19.055));
            CHECK(test::near(on[i], 20.675));
        }
    }
    const auto building = makeRingBuildingDynamics(4);
    const auto actual = building->step({17,18,19,20}, {0,0,0,0}, {});
    const Vector expected{17.99,17.145,18.1,17.255};
    for (std::size_t i = 0; i < expected.size(); ++i) CHECK(test::near(actual[i], expected[i]));
}

void bounds() {
    const auto thermostat = makeThermostatDynamics();
    const Box temperatures{{17}, {23}}, weather{{10}, {15}};
    for (double input : {0.0, 1.0}) {
        CHECK(thermostat->cooperativeOn(temperatures, {input}, weather));
        const auto derivative = thermostat->absoluteJacobianBounds(temperatures, {input}, weather);
        validateDerivativeBounds(derivative, 1, 1);
        CHECK(test::near(derivative.Dx[0][0], 0.9-0.2*input));
        CHECK(test::near(derivative.Dw[0][0], 0.1));
    }
    CHECK(!thermostat->cooperativeOn(temperatures, {5}, weather));
    CHECK(test::near(thermostat->absoluteJacobianBounds(temperatures, {5}, weather).Dx[0][0], 0.1));
    const auto building = makeRingBuildingDynamics(4);
    const Box states{Vector(4,17), Vector(4,22)};
    const Vector inputs{0,0.3,0.6,1};
    CHECK(!building->cooperativeOn(states, inputs, {{},{}}));
    CHECK(building->cooperativeOn(states, Vector(4,0.6), {{},{}}));
    const auto derivative = building->absoluteJacobianBounds(states, inputs, {{},{}});
    validateDerivativeBounds(derivative, 4, 0);
    for (std::size_t i = 0; i < 4; ++i) {
        for (std::size_t j = 0; j < 4; ++j) {
            const double expected = i == j ? std::abs(0.055-0.09*inputs[i])
                : ((j == (i+1)%4 || j == (i+3)%4) ? 0.45 : 0);
            CHECK(test::near(derivative.Dx[i][j], expected));
        }
        CHECK(derivative.Dw[i].empty());
    }
}

void invalid() {
    test::throws<std::invalid_argument>([] { makeRingBuildingDynamics(2); });
    test::throws<std::length_error>([] { makeRingBuildingDynamics(std::numeric_limits<std::size_t>::max()); });
    const auto thermostat = makeThermostatDynamics();
    const auto building = makeRingBuildingDynamics(3);
    test::throws<std::invalid_argument>([&] { thermostat->step({}, {0}, {10}); });
    test::throws<std::invalid_argument>([&] { thermostat->step({18}, {}, {10}); });
    test::throws<std::invalid_argument>([&] { thermostat->step({18}, {0}, {}); });
    test::throws<std::invalid_argument>([&] { building->step({20,20,20}, {0,0,0}, {0}); });
    test::throws<std::invalid_argument>([&] { building->step({20,20}, {0,0,0}, {}); });
    test::throws<std::invalid_argument>([&] { thermostat->cooperativeOn({{23},{17}}, {0}, {{10},{15}}); });
    test::throws<std::invalid_argument>([&] { building->absoluteJacobianBounds({{17},{22}}, {0,0,0}, {{},{}}); });
    const double nan = std::numeric_limits<double>::quiet_NaN();
    test::throws<std::invalid_argument>([&] { thermostat->step({nan}, {0}, {10}); });
    test::throws<std::invalid_argument>([&] { building->cooperativeOn({{17,17,17},{22,22,22}}, {0,nan,0}, {{},{}}); });
}

void extremes() {
    const double limit = std::numeric_limits<double>::max();
    const auto thermostat = makeThermostatDynamics();
    const auto building = makeRingBuildingDynamics(3);
    const auto next = thermostat->step({limit}, {1}, {limit});
    CHECK(std::isfinite(next[0]) && next[0] > 0);
    for (double value : building->step(Vector(3,limit), Vector(3,0), {}))
        CHECK(std::isfinite(value) && value > 0);
    test::throws<std::overflow_error>([&] { thermostat->step({limit}, {-limit}, {0}); });
    test::throws<std::overflow_error>([&] { building->step(Vector(3,limit), Vector(3,-limit), {}); });
}
}

int main(int argc, char** argv) {
    return test::run(argc, argv, {{"equations", equations}, {"bounds", bounds},
        {"invalid", invalid}, {"extremes", extremes}});
}
