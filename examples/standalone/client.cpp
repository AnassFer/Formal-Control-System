#include "fixture.hpp"
#include <iostream>
int main() {
    const auto model=symctrl::fixtures::nondeterministicChoice();
    std::cout << model.metadata.model_id << " M=" << model.transitions->internalStateCount()
              << " unsafe_successor_3=" << symctrl::contains(model.transitions->successors(1,0),3,3) << '\n';
}
