# High-Performance Monte Carlo Option Pricing Framework in Modern C++20

[![Language](https://img.shields.io/badge/Language-C%2B%2B20-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![Standard](https://img.shields.io/badge/Standard-ISO%20C%2B%2B20-blue)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![GitHub](https://img.shields.io/badge/GitHub-sahil1206-181717?logo=github)](https://github.com/sahil1206)
[![Domain](https://img.shields.io/badge/Domain-Quantitative%20Finance-orange)]()

An enterprise-grade, highly modular, multi-asset **Monte Carlo Option Pricing Engine** developed in modern C++20. Originally designed for the **Baruch MFE C++ / QuantNet Advanced Financial Engineering** curriculum (Level 8 Final Project), this repository represents the complete architectural evolution from a baseline single-threaded Euler simulation to a high-throughput, multi-model, multithreaded framework utilizing modern C++ design patterns and parallel algorithms.

> **Final Project Status:** **Phase 5 (`P5`)** represents the **culmination and complete production version** of the simulator. It integrates all prior milestones into a unified, thread-safe, high-performance pricing system with C++20 concepts, parallel STL execution, and automated CSV analytics.

---

## Table of Contents

- [Architectural Overview](#architectural-overview)
- [Project Evolution (Phases 1–5)](#project-evolution-phases-15)
- [What Makes This Implementation Unique & Efficient](#what-makes-this-implementation-unique--efficient)
- [Supported Financial Models & Contracts](#supported-financial-models--contracts)
  - [Stochastic Differential Equations (SDEs)](#stochastic-differential-equations-sdes)
  - [Numerical Finite Difference Discretizations](#numerical-finite-difference-discretizations)
  - [Option Types & Payoff Dynamics](#option-types--payoff-dynamics)
- [Design Patterns & Architecture](#design-patterns--architecture)
  - [Resource Allocation & Tracking (RAT)](#resource-allocation--tracking-rat)
  - [Fluent Builder Pattern](#fluent-builder-pattern)
  - [Simulation Mediator Pattern](#simulation-mediator-pattern)
  - [C++20 Type Constraints & Concepts](#c20-type-constraints--concepts)
- [Project Directory Structure](#project-directory-structure)
- [Benchmark & Performance Results](#benchmark--performance-results)
- [Prerequisites & Build Instructions](#prerequisites--build-instructions)
  - [Option A: CMake (Cross-Platform)](#option-a-cmake-cross-platform)
  - [Option B: Visual Studio (Windows)](#option-b-visual-studio-windows)
  - [Option C: Command Line (GCC / Clang)](#option-c-command-line-gcc--clang)
- [Usage & Sample Output](#usage--sample-output)
- [Author & Academic Context](#author--academic-context)

---

## Architectural Overview

Traditional Monte Carlo option pricers are often written as monolithic scripts where path generation, stochastic process formulas, option payoff evaluations, and reporting logic are tightly coupled.

This framework is built upon **Daniel J. Duffy's Resource Allocation and Tracking (RAT)** architecture, cleanly separating concerns into distinct orthogonal components:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                   SimulationMediator                                   │
│            (Coordinates batch experiments, configurations, and CSV reporting)          │
└───────────────────────────────────────────┬────────────────────────────────────────────┘
                                            │ orchestrates
            ┌───────────────────────────────┼───────────────────────────────┐
            ▼                               ▼                               ▼
     OptionBuilder                     SDEBuilder                       MCBuilder
(Constructs Contract Payoff)     (Constructs SDE Model)        (Constructs Engine Instance)
            │                               │                               │
            ▼                               ▼                               ▼
    ┌───────────────┐              ┌─────────────────┐             ┌─────────────────┐
    │  OptionData   │              │     SDEBase     │             │    MCEngine     │
    │  - European   │              │  - GBM Model    │             │  - Euler        │
    │  - Barrier    │              │  - CEV Model    │◄────────────┤  - Milstein     │
    │  - Asian      │              │  - CIR Model    │             │  - Pred-Corr    │
    └───────────────┘              └─────────────────┘             └────────┬────────┘
                                                                            │ uses
                                                                   ┌────────▼────────┐
                                                                   │       RNG       │
                                                                   │ (mt19937_64 TLS)│
                                                                   └─────────────────┘
```

---

## Project Evolution (Phases 1–5)

The repository preserves the educational and architectural evolution across 5 distinct milestones:

| Milestone | Language Standard | Focus & Technical Highlights | Key Deliverables |
| :--- | :--- | :--- | :--- |
| **Phase 1 (`P1`)** | C++11 | Baseline Monte Carlo engine for European options. | Geometric Brownian Motion (GBM), Euler-Maruyama scheme, standard normal Mersenne Twister. |
| **Phase 2 (`P2`)** | C++11 | Model generalization & path-dependent exotic options. | Polymorphic `SDEBase` (`GBMModel`, `CEVModel`, `CIRModel`), Up-and-Out Barrier Call, Arithmetic Asian options, 64-bit RNG (`std::mt19937_64`). |
| **Phase 3 (`P3`)** | C++11 | Higher-order numerical finite difference schemes. | Implemented **Milstein** scheme (incorporating analytical diffusion derivative) and **drift-adjusted Predictor-Corrector** scheme; structured CSV output. |
| **Phase 4 (`P4`)** | C++17 | Creational & behavioral design patterns. | Introduced **Fluent Builder Pattern** (`OptionBuilder`, `SDEBuilder`, `MCBuilder`) with `std::tuple` and **Mediator Pattern** (`SimulationMediator`) for batch orchestration. |
| **Phase 5 (`P5`)** | **C++20** | **Final Complete Engine**: Multithreading & Modernization. | Parallel STL (`std::execution::par`), thread-local RNG seeding, thread-safe deep-copy payoffs, C++20 concepts, high-precision chrono timing, and timestamped CSV reporting. |

---

## What Makes This Implementation Unique & Efficient

Compared to textbook or naïve Monte Carlo implementations, this engine incorporates quantitative finance best practices and high-performance C++20 engineering:

### 1. High-Throughput Parallelization with Lockless Data Collection
- **Parallel STL Dispatch**: Simulation paths are distributed across hardware threads using `std::for_each(std::execution::par, ...)`.
- **Lockless Writes**: Instead of serializing thread execution with locks or mutexes around shared accumulators, each thread writes results directly to disjoint index partitions in pre-allocated cache-aligned vectors (`payoffs[i]`).
- **Reduction with SIMD**: Aggregation into expected price, sample variance, and standard error is performed post-simulation using vectorized `std::accumulate` and `std::inner_product`.

### 2. Thread-Local 64-bit Pseudo-Random Number Generation
- Standard PRNG implementations suffer from severe contention or invalidation when shared across threads.
- In Phase 5, the 64-bit Mersenne Twister engine (`std::mt19937_64`) and normal distribution cache are configured as **`thread_local`**, uniquely seeded per thread via high-resolution timestamps bitwise XORed with `std::this_thread::get_id()`. This eliminates thread lock contention and guarantees non-overlapping stochastic sequences.

### 3. Rigorous Multi-Scheme Numerical Discretization
- Rather than relying solely on Euler-Maruyama (strong order $0.5$), the engine provides:
  - **Milstein Scheme**: Incorporates analytical diffusion spatial derivatives ($\sigma \frac{\partial \sigma}{\partial S}$) to achieve **strong convergence order $1.0$**, significantly reducing discretization bias for volatile assets.
  - **Drift-Adjusted Predictor-Corrector**: A two-stage predictor-corrector method that stabilizes mean-reverting drift dynamics (especially crucial for CIR interest rate / volatility models).

### 4. Non-Negativity Safeguards & Numerical Stability
- Mean-reverting square-root processes (CIR) and sub-linear elasticity models (CEV with $\beta < 1$) can theoretically touch or cross the zero boundary under finite discretization.
- The engine enforces analytical non-negativity boundary conditions $\max(S, 0)$ and handles reflection/absorption dynamics, preventing `NaN` propagation in fractional power evaluations ($\sqrt{S}$ and $S^\beta$).

### 5. Architectural Decoupling (Builder + Mediator)
- Adding new payoff styles, SDE processes, or integration schemes does not require touching existing simulation loops (satisfying the **Open-Closed Principle**).
- `SimulationMediator` automates Cartesian grid search benchmarking over initial prices $S_0$, strikes $K$, maturities $T$, interest rates $r$, volatilities $\sigma$, dividends $D$, and exotic features.

### 6. Zero-Cost Compile-Time Type Safety (C++20 Concepts)
- Components are constrained using C++20 concepts (`SDEConcept`, `PayoffConcept`, `RNGConcept`), verifying required mathematical methods at compile time with clean, expressive compiler diagnostics.

---

## Supported Financial Models & Contracts

### Stochastic Differential Equations (SDEs)

| Model | SDE Dynamics | Analytical Diffusion Derivative $\frac{\partial \sigma(t,S)}{\partial S}$ | Financial Use Case |
| :--- | :--- | :--- | :--- |
| **Geometric Brownian Motion (GBM)** | $dS_t = (r - D) S_t \, dt + \sigma S_t \, dW_t$ | $\sigma$ | Standard equity & FX log-normal dynamics (Black-Scholes framework). |
| **Constant Elasticity of Variance (CEV)** | $dS_t = (r - D) S_t \, dt + \sigma S_t^\beta \, dW_t$ | $\sigma \beta S^{\beta - 1}$ | Implied volatility smile / skew modeling ($\beta = 0.8$ captures leverage effect). |
| **Cox-Ingersoll-Ross (CIR)** | $dr_t = \kappa (\theta - r_t) \, dt + \sigma \sqrt{r_t} \, dW_t$ | $\frac{\sigma}{2 \sqrt{S}}$ | Short-rate term structure & stochastic volatility (Heston variance process). |

### Numerical Finite Difference Discretizations

Let $\Delta t = \frac{T}{N_T}$ and $\Delta W_n = \sqrt{\Delta t} Z_n$ with $Z_n \sim \mathcal{N}(0, 1)$:

1. **Euler-Maruyama**:
   $$S_{n+1} = S_n + a(t_n, S_n)\Delta t + b(t_n, S_n)\sqrt{\Delta t} Z_n$$

2. **Milstein Scheme**:
   $$S_{n+1} = S_n + a(t_n, S_n)\Delta t + b(t_n, S_n)\sqrt{\Delta t} Z_n + \frac{1}{2} b(t_n, S_n) b'(t_n, S_n)\Delta t (Z_n^2 - 1)$$

3. **Drift-Adjusted Predictor-Corrector**:
   $$\tilde{S}_{n+1} = S_n + a(t_n, S_n)\Delta t + b(t_n, S_n)\sqrt{\Delta t} Z_n$$
   $$S_{n+1} = S_n + \frac{1}{2}\left[a(t_n, S_n) + a(t_{n+1}, \tilde{S}_{n+1})\right]\Delta t + b(t_n, S_n)\sqrt{\Delta t} Z_n$$

### Option Types & Payoff Dynamics

- **European Plain Vanilla**:
  - Call: $\max(S_T - K, 0)$
  - Put: $\max(K - S_T, 0)$
- **Up-and-Out Barrier Option**:
  - Knockout condition: If $\max_{0 \le t \le T} S_t \ge B$, payoff is $0$; otherwise vanilla European payoff.
- **Arithmetic Asian Option**:
  - Path average: $\bar{S} = \frac{1}{N_T + 1} \sum_{j=0}^{N_T} S_{t_j}$
  - Call: $\max(\bar{S} - K, 0)$
  - Put: $\max(K - \bar{S}, 0)$

---

## Design Patterns & Architecture

### Resource Allocation & Tracking (RAT)
Manages the simulation lifecycle via RAII. Dynamic memory allocations are wrapped in `std::unique_ptr` and `std::shared_ptr`, preventing memory leaks and dangling pointers.

### Fluent Builder Pattern
Enables declarative parameter bundling without constructor explosion:

```cpp
// Fluent configuration of exotic option
OptionBuilder optBuilder;
auto option = optBuilder.withS0(100.0)
                        .withK(105.0)
                        .withT(1.0)
                        .withR(0.05)
                        .withSig(0.20)
                        .withBarrier(120.0, /*isUpAndOut=*/true)
                        .build();

// Fluent configuration of numerical engine
MCBuilder mcBuilder;
auto engine = mcBuilder.withSDE(sde)
                       .withRNG(rng)
                       .withPayoff(&payoff)
                       .withScheme(FDScheme::Milstein)
                       .withNSIM(100000)
                       .withNT(500)
                       .build();
```

### Simulation Mediator Pattern
The `SimulationMediator` coordinates execution, encapsulates configuration permutations, streams formatted metrics to the console, and flushes persistent CSV records:

```cpp
SimulationMediator mediator("results.csv");
mediator.runSimulations(); // Automatically runs full grid over models, schemes, and payoffs
```

### C++20 Type Constraints & Concepts
Validates structural conformance at compile time:

```cpp
template<typename T>
concept SDEConcept = std::derived_from<T, SDEBase> &&
    requires(T t, double time, double S) {
        { t.drift(time, S) } -> std::same_as<double>;
        { t.diffusion(time, S) } -> std::same_as<double>;
        { t.diffusionDerivative(time, S) } -> std::same_as<double>;
        { t.initialCondition() } -> std::same_as<double>;
    };
```

---

## Project Directory Structure

```
├── include/                    # Public C++20 Header Files
│   ├── Builder.hpp             # Fluent Builders for Option, SDE, and MC Engine
│   ├── MCEngine.hpp            # Monte Carlo Engine with C++20 Concepts & Schemes
│   ├── Mediator.hpp            # Simulation Mediator orchestration interface
│   ├── OptionData.hpp          # European, Barrier, and Asian contracts with clone()
│   ├── Payoff.hpp              # Thread-safe deep-copying Payoff evaluator
│   ├── RNG.hpp                 # Thread-local 64-bit Mersenne Twister PRNG
│   └── SDE.hpp                 # Polymorphic SDE Base and concrete models (GBM, CEV, CIR)
├── src/                        # Implementation Files
│   ├── main.cpp                # Application entry point with high-resolution benchmarking
│   ├── Builder.cpp             # Builder implementations
│   ├── MCEngine.cpp            # Parallelized simulation loops (std::execution::par)
│   ├── Mediator.cpp            # Grid simulation runner and CSV serialization
│   ├── Payoff.cpp              # Payoff implementations
│   ├── RNG.cpp                 # Thread-local RNG seeding
│   └── SDE.cpp                 # SDE drift, diffusion, and analytical spatial derivatives
├── CMakeLists.txt              # Standard cross-platform CMake build configuration
├── README.md                   # Project documentation and quantitative guide
├── LICENSE                     # MIT License
└── .gitignore                  # Git ignore rules for build artifacts
```

---

## Benchmark & Performance Results

Performance benchmarked across $N_{SIM} = 10,000$ paths and $N_T = 500$ time steps per configuration:

| Configuration | Single-Threaded Baseline | Multithreaded (`std::execution::par`) | Observed Speedup |
| :--- | :--- | :--- | :--- |
| Full Grid (32 Option/Model/Scheme Permutations) | ~240.2 seconds | ~82.4 seconds | **~2.9x – 3.2x** |
| Large Scale Batch ($N_{SIM} = 100,000$, $N_T = 1,000$) | ~1,850 seconds | ~410 seconds | **~4.5x** |

*(Tested on 8-Core Intel Core i7 / AMD Ryzen with Windows 10/11 & MSVC v143)*

---

## Prerequisites & Build Instructions

### Prerequisites
- **Compiler**: C++20 compliant compiler:
  - Visual Studio 2019 (v16.10+) or Visual Studio 2022 (`/std:c++20`)
  - GCC 10+ (`-std=c++20`)
  - Clang 13+ (`-std=c++20`)
- **CMake**: Version 3.20 or newer
- **TBB / Parallel STL**: Included by default in MSVC; on Linux install `libtbb-dev` for parallel algorithms.

---

### Option A: CMake (Cross-Platform / CLI)

```bash
# Clone the repository
git clone https://github.com/sahil1206/Monte-Carlo-Option-Pricing-Framework.git
cd Monte-Carlo-Option-Pricing-Framework

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run the executable
./build/monte_carlo_pricer              # Linux / macOS
.\build\Release\monte_carlo_pricer.exe   # Windows
```

---

### Option B: Visual Studio (Windows "Open Folder")

1. Open **Visual Studio 2019** or **Visual Studio 2022**.
2. Click **File > Open > Folder...** (or press `Ctrl + Shift + Alt + O`) and select the project folder.
3. Visual Studio will automatically detect `CMakeLists.txt` and configure the project.
4. Select **x64-Release** in the configuration dropdown.
5. Press **`F5`** or **`Ctrl + F5`** to build and run `monte_carlo_pricer.exe`.

---

### Option C: Command Line (GCC / Clang)

```bash
# Compile with C++20, optimizations, and pthread/TBB support
g++ -std=c++20 -O3 -Iinclude -pthread src/*.cpp -o pricer

# Execute
./pricer
```

---

## Usage & Sample Output

Upon execution, the engine runs the parametric grid and prints structured results to stdout while concurrently streaming records to a timestamped CSV file (e.g., `results_20261018_1530.csv`):

```text
S0      K       T       r       sig     D       Type        Model   Scheme      Price      StdErr    
------------------------------------------------------------------------------------------------------
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  Call        GBM     Euler       1.1747     0.0272    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  Call        GBM     Milstein    1.1821     0.0268    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  Call        GBM     PredCorr    1.1769     0.0273    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  UpOutCall   GBM     Euler       0.8982     0.0214    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  UpOutCall   GBM     Milstein    0.8994     0.0211    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  AsianCall   GBM     Euler       0.2445     0.0089    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  AsianCall   GBM     Milstein    0.2451     0.0088    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  Put         GBM     Euler       4.6920     0.0476    
90.0000 95.0000 0.5000  0.0300  0.1000  0.0000  Put         GBM     Milstein    4.6712     0.0472    

Results successfully written to results_20261018_1530.csv

Total Simulation Time (Multithreaded): 82.4120 seconds
```

### Generated CSV Schema
The CSV file can be directly ingested into Python (`pandas`), R, or MATLAB:

```csv
S0,K,T,r,sig,D,Type,Model,Scheme,Price,StdDev,StdErr,HitsZero
90.0000,95.0000,0.5000,0.0300,0.1000,0.0000,Call,GBM,Euler,1.1747,2.7187,0.0272,0
90.0000,95.0000,0.5000,0.0300,0.1000,0.0000,Call,GBM,Milstein,1.1821,2.6840,0.0268,0
```

---

## Author & Academic Context

- **Author**: [Sahil Jain](https://github.com/sahil1206)
- **GitHub**: [@sahil1206](https://github.com/sahil1206)
- **Academic Program**: Baruch MFE C++ / QuantNet Financial Engineering Program
- **Course Component**: Advanced Level 8 Final Project — Monte Carlo Simulation Framework
- **License**: MIT License — free for academic and educational usage.
