#pragma once
#include "symctrl/contracts/types.hpp"
namespace symctrl {
class IStateGrid {
public:
    virtual ~IStateGrid() = default;
    virtual std::size_t stateDimension() const noexcept = 0;
    virtual const GridSpec& spec() const noexcept = 0;
    virtual StateId internalStateCount() const noexcept = 0;
    virtual StateId locate(const Vector& x) const = 0;
    virtual Box cellBounds(StateId state) const = 0;
    virtual GridIndex indices(StateId state) const = 0;
    virtual StateId fromIndices(const GridIndex& index) const = 0;
    virtual std::vector<StateId> intersectingInternalCells(const Box& closed_box) const = 0;
};
}
