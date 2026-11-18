#pragma once
#include "symctrl/types.hpp"
namespace symctrl {
class IInputCatalog {
public:
    // Input: None; called when this input catalog is destroyed.
    // Output: No return value; permits destruction of the concrete object through this interface.
    virtual ~IInputCatalog() = default;
    // Input: None; reads this input catalog.
    // Output: Number m of coordinates in each physical input vector.
    virtual std::size_t inputDimension() const noexcept = 0;
    // Input: None; reads this input catalog.
    // Output: Number A of available actions; valid action IDs are 0..A-1.
    virtual InputId inputCount() const noexcept = 0;
    // Input: input: action ID in 0..A-1.
    // Output: Read-only reference to the corresponding m-coordinate vector, valid for the catalog lifetime.
    // Throws: std::out_of_range for an invalid action ID.
    virtual const Vector& value(InputId input) const = 0; // Valid for catalogue lifetime.
};
}
