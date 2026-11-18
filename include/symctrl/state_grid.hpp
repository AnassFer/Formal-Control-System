#pragma once
#include "symctrl/types.hpp"
namespace symctrl {
class IStateGrid {
public:
    // Input: None; called when this grid is destroyed.
    // Output: No return value; permits destruction of the concrete object through this interface.
    virtual ~IStateGrid() = default;
    // Input: None; reads this grid.
    // Output: Number n of continuous state coordinates.
    virtual std::size_t stateDimension() const noexcept = 0;
    // Input: None; reads this grid.
    // Output: Read-only reference to domain and cell counts, valid for the grid lifetime.
    virtual const GridSpec& spec() const noexcept = 0;
    // Input: None; reads this grid.
    // Output: Number M of internal cells; OUT (state 0) is excluded.
    virtual StateId internalStateCount() const noexcept = 0;
    // Input: x: finite physical state vector with n coordinates.
    // Output: Owning state ID in 1..M, or OUT (0) outside the domain; internal faces belong to the
    //         higher-index cell.
    // Throws: std::invalid_argument for a wrong dimension or nonfinite coordinate.
    virtual StateId locate(const Vector& x) const = 0;
    // Input: state: internal cell ID in 1..M.
    // Output: Closed cell box with n lower and n upper coordinates.
    // Throws: std::out_of_range for OUT or an invalid state ID.
    virtual Box cellBounds(StateId state) const = 0;
    // Input: state: internal cell ID in 1..M.
    // Output: Zero-based index per coordinate, with coordinate 0 varying fastest.
    // Throws: std::out_of_range for OUT or an invalid state ID.
    virtual GridIndex indices(StateId state) const = 0;
    // Input: index: n zero-based coordinate indices, each below its cell count.
    // Output: Internal state ID 1 + sum(index[d] * stride[d]) in 1..M.
    // Throws: std::invalid_argument for a wrong index dimension; std::out_of_range for an invalid component.
    virtual StateId fromIndices(const GridIndex& index) const = 0;
    // Input: closed_box: finite closed box with n coordinates.
    // Output: Sorted unique internal IDs whose closed cells intersect the box; may be empty; excludes OUT.
    // Throws: std::invalid_argument for an invalid box or dimension mismatch.
    virtual std::vector<StateId> intersectingInternalCells(const Box& closed_box) const = 0;
};
}
