# Eigen Get Started Example

This example is a small introduction to using [Eigen](https://libeigen.gitlab.io/) from an ESP-IDF C++ component.
It does not require any special hardware; the program only performs calculations and prints the results to the serial console.

## What you will learn

The example follows a simple progression:

1. Create fixed-size matrices and vectors, and perform basic operations.
   The output identifies matrix-vector multiplication, the determinant, and the Euclidean vector norm.
2. Solve a linear system of equations:

   $$A x = b$$

   The serial output shows the problem first (the coefficient matrix `A`, right-hand side `b`, and the corresponding equations), then the solution `x`, and finally the residual `r = A x - b`.
   A residual close to zero means that the computed solution reproduces `b` accurately.
   The example uses QR decomposition, rather than explicitly calculating `A.inverse()`, to solve the system.

3. Fit a simple sensor calibration model to measured data. The model is:

   $$y = a x + b$$

   For several measurements, this can be written as a least-squares problem:

   $$X \binom{a}{b} \approx y$$

   The example solves this problem using QR decomposition and also demonstrates SVD as an alternative solver.
   It prints the fitting error so that the result can be checked.
4. Rotate a 3D point with a quaternion.
   The output identifies the rotation axis and angle, and checks that rotation preserves the point's length.
5. Compare fixed-size and dynamic-size matrices, including their dimensions and the meaning of the trace.
   The example uses fixed-size matrices where the dimensions are known at compile time.
   This is often a good choice for small embedded control and sensor-processing problems.
   Dynamic-size matrices are also shown for cases where dimensions are only known at runtime.

## Build and run

Run the commands from this directory:

```bash
cd eigen/examples/get_started
idf.py set-target esp32
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port of your board. The example can also be built for other supported ESP-IDF targets, for example `esp32s3` or `esp32c3`.

The serial output is organized into sections matching the concepts above.
In the sensor calibration section, a small fitting error indicates that the model was successfully fitted to the sample measurements.

## Continue learning

This example intentionally introduces only a few common Eigen APIs.
For detailed API documentation and additional modules, see the [Eigen documentation](https://libeigen.gitlab.io/).
