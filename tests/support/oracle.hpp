#pragma once
#include "symctrl/contracts/policy.hpp"
#include <array>
namespace test {
struct ThermostatRow { symctrl::StateId state; symctrl::InputId input; symctrl::Box enclosure; std::vector<symctrl::StateId> ids; };
inline const std::array<ThermostatRow,6> thermostat_oracle{{
    {1,0,{{16.3},{18.6}},{0,1}}, {1,1,{{18.9},{20.8}},{1,2}},
    {2,0,{{18.1},{20.4}},{1,2}}, {2,1,{{20.3},{22.2}},{2,3}},
    {3,0,{{19.9},{22.2}},{2,3}}, {3,1,{{21.7},{23.6}},{0,3}}
}};
class SuppliedPolicy final : public symctrl::ISymbolicPolicy {
public:
    symctrl::PolicyDecision decide(symctrl::PolicyMemory memory, symctrl::StateId state) const override {
        if (state < 1 || state > 3) throw std::out_of_range("manual policy domain");
        return {memory, state == 1 ? symctrl::InputId{1} : symctrl::InputId{0}};
    }
};
}
