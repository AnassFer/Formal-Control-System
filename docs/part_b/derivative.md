# Bounded-derivative reachability

The backend selected by ReachabilityMethod::BoundedDerivative uses IDynamics::absoluteJacobianBounds on the full requested state/disturbance boxes for the fixed physical input. It needs no cooperative-monotonicity capability.

For centres xc and wc and componentwise radii rx and rw, it computes c=f(xc,u,wc), r=Dx*rx+Dw*rw and [c-r,c+r], as described in paper Section 1.1.3 (PDF page 6). Bounds must be finite, nonnegative and shaped n by n and n by q. With q=0, Dw has n empty rows and the disturbance vector is empty.

Midpoints avoid overflowing the sum/difference of finite extreme endpoints. Radii use the larger distance to the computed midpoint, including subnormal intervals where the exact midpoint cannot be represented. Nonfinite computed products, sums or output endpoints raise overflow_error. Malformed requests/bounds/callback outputs raise invalid_argument; model exceptions propagate. These checks do not enclose all floating-point rounding errors: the backend reports UncertifiedFloat and rejects require_validated=true.

The backend retains immutable shared ownership of its supplied dynamics. Focused tests link only B/common and shared test support:

```sh
cmake --preset dev
cmake --build build/dev --target derivative_reachability_tests --parallel 2
ctest --test-dir build/dev -R '^part_b\.derivative\.' --output-on-failure --no-tests=error
```

The eleven non-skipping cases cover signed affine dynamics, generic dimensions, q=0, point boxes, full-box nonlinear derivative capabilities, invalid requests/matrices/results, factory validation, lifetime, finite extreme/subnormal boxes, overflow and explicit model failures.
