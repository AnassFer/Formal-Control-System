#pragma once
#include "symctrl/contracts/types.hpp"
namespace symctrl {
class IInputCatalog {
public:
    virtual ~IInputCatalog() = default;
    virtual std::size_t inputDimension() const noexcept = 0;
    virtual InputId inputCount() const noexcept = 0;
    virtual const Vector& value(InputId input) const = 0; // Valid for catalogue lifetime.
};
}
