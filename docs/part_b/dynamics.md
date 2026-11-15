# Existing model adapters

The generic reachability/transition pipeline receives IDynamics; it contains no model-specific constants. Two adapters preserve the current public factories and regression tests.

## Thermostat

The supplied paper's model is f(x,u,w) = x + 0.1(w-x) + 0.2u(30-x), evaluated in affine form. Its state derivative is 0.9-0.2u and disturbance derivative is 0.1. These derivatives are constant over every requested box. cooperativeOn is true exactly when the state coefficient is nonnegative. Absolute derivative bounds use the absolute state coefficient and 0.1. The intended example selects u=0 or 1 and w in [10,15].

## Circular building

The supplied project slides (printed slide 14, PDF page 15) define N>=3 rooms, alpha=0.45, beta=0.045, gamma=0.09, Te=-1 and Th=50. Circular neighbours have coefficient alpha. Room i has self coefficient 1-2*alpha-beta-gamma*u[i] and constant beta*Te+gamma*Th*u[i]. All self coefficients are positive for the intended input interval [0,0.6], so the model is cooperative on that interval. The absolute Jacobian contains the absolute self coefficient and the two neighbour coefficients; its disturbance Jacobian has N empty rows because q=0.

Both adapters validate finite values, vector/box shapes and bound ordering. The supplied grid/input configuration determines physical domains and selected actions; adapters accept other finite input values and return the mathematically appropriate capability/bounds. For example, larger heater inputs can make a self coefficient negative, in which case the monotone method rejects that request and the derivative method remains applicable.

Updates use wider intermediate arithmetic to avoid avoidable overflow from temperature differences. Unrepresentable double results raise overflow_error. All reachability results remain UncertifiedFloat. Building derivative-matrix arithmetic/container sizes are checked before allocation; no large benchmark is required for these adapters.

Independent checks are registered as part_b.dynamics.* from tests/part_b/dynamics.cpp. They cover the supplied equations, configurable room counts and circular neighbours, full-box derivative bounds, unsupported monotonicity, invalid shapes/nonfinite values, and finite-extreme/overflow behaviour. They link no A or fixture implementation.
