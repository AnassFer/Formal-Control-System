#pragma once
#include "symctrl/contracts/model_bundle.hpp"
namespace symctrl::fixtures {
// Tiny fixed hand-authored records. No production factories or dynamics are used.
ModelBundle thermostat();
ModelBundle nondeterministicChoice();
}
