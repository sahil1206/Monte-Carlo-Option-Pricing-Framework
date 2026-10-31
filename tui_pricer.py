#!/usr/bin/env python3
"""
Interactive Terminal User Interface (TUI) for Monte Carlo Option Pricing
Companion tool for C++20 Monte Carlo Simulation Framework (Baruch MFE Level 8)
Author: Sahil Jain
"""

import math
import random
import time
import datetime
import sys
import os
import csv

# ANSI Colors
class Color:
    RESET   = "\033[0m"
    BOLD    = "\033[1m"
    DIM     = "\033[2m"
    CYAN    = "\033[36m"
    GREEN   = "\033[32m"
    YELLOW  = "\033[33m"
    MAGENTA = "\033[35m"
    BLUE    = "\033[34m"
    RED     = "\033[31m"
    GRAY    = "\033[90m"

# Enable ANSI escape sequences on Windows
if os.name == 'nt':
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    except Exception:
        pass

def normal_cdf(x: float) -> float:
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))

def black_scholes_price(S0: float, K: float, T: float, r: float, sig: float, D: float, option_type: int) -> float:
    if T <= 0.0 or sig <= 0.0:
        return max(S0 - K, 0.0) if option_type == 1 else max(K - S0, 0.0)
    sqrt_T = math.sqrt(T)
    d1 = (math.log(S0 / K) + (r - D + 0.5 * sig * sig) * T) / (sig * sqrt_T)
    d2 = d1 - sig * sqrt_T
    if option_type == 1:  # Call
        return S0 * math.exp(-D * T) * normal_cdf(d1) - K * math.exp(-r * T) * normal_cdf(d2)
    else:  # Put
        return K * math.exp(-r * T) * normal_cdf(-d2) - S0 * math.exp(-D * T) * normal_cdf(-d1)

class Config:
    def __init__(self):
        # Asset Model: "GBM", "CEV", "CIR"
        self.model_type = "GBM"
        self.cev_beta = 0.8
        self.cir_kappa = 0.5
        self.cir_theta = 99.0

        # Contract
        self.contract_label = "European Call"
        self.option_type = 1  # 1 = Call, -1 = Put
        self.is_barrier = False
        self.barrier = 108.0
        self.is_up_and_out = True
        self.is_asian = False

        # Market & Contract Parameters
        self.S0 = 90.0
        self.K = 95.0
        self.T = 0.5
        self.r = 0.03
        self.sig = 0.10
        self.D = 0.00

        # Numerical Scheme: "Euler", "Milstein", "PredictorCorrector"
        self.scheme = "Euler"

        # Simulation Setup
        self.NSIM = 10000
        self.NT = 500
        self.fixed_seed = False
        self.seed_value = 42
        self.antithetic = False

class Simulator:
    @staticmethod
    def drift(model_type: str, t: float, S: float, cfg: Config) -> float:
        if model_type == "CIR":
            return cfg.cir_kappa * (cfg.cir_theta - S)
        # GBM and CEV
        return (cfg.r - cfg.D) * S

    @staticmethod
    def diffusion(model_type: str, t: float, S: float, cfg: Config) -> float:
        S_clamped = max(S, 0.0)
        if model_type == "GBM":
            return cfg.sig * S
        elif model_type == "CEV":
            return cfg.sig * (S_clamped ** cfg.cev_beta)
        elif model_type == "CIR":
            return cfg.sig * math.sqrt(S_clamped)
        return cfg.sig * S

    @staticmethod
    def diffusion_derivative(model_type: str, t: float, S: float, cfg: Config) -> float:
        S_clamped = max(S, 0.0001)
        if model_type == "GBM":
            return cfg.sig
        elif model_type == "CEV":
            return cfg.sig * cfg.cev_beta * (S_clamped ** (cfg.cev_beta - 1.0))
        elif model_type == "CIR":
            return cfg.sig / (2.0 * math.sqrt(S_clamped))
        return cfg.sig

    @classmethod
    def run_simulation(cls, cfg: Config) -> dict:
        start_time = time.perf_counter()

        if cfg.fixed_seed:
            random.seed(cfg.seed_value)

        T = cfg.T
        NT = cfg.NT
        k = T / float(NT)
        sqrk = math.sqrt(k)
        r = cfg.r
        NSIM = cfg.NSIM

        def step_path(S: float, dW: float, t: float, model: str, scheme: str) -> float:
            d = cls.drift(model, t, S, cfg)
            diff = cls.diffusion(model, t, S, cfg)

            if scheme == "Euler":
                S += k * d + sqrk * diff * dW
            elif scheme == "Milstein":
                diff_deriv = cls.diffusion_derivative(model, t, S, cfg)
                S += k * d + sqrk * diff * dW + 0.5 * diff * diff_deriv * k * (dW * dW - 1.0)
            elif scheme == "PredictorCorrector":
                S_pred = S + k * d + sqrk * diff * dW
                if model == "CIR" and S_pred < 0.0:
                    S_pred = 0.0
                d_pred = cls.drift(model, t + k, S_pred, cfg)
                S += 0.5 * k * (d + d_pred) + sqrk * diff * dW

            if model == "CIR" and S < 0.0:
                S = 0.0
            return S

        def compute_payoff(final_S: float, price_sum: float, steps: int, knocked_out: bool) -> float:
            if cfg.is_barrier:
                if knocked_out:
                    return 0.0
                return max(final_S - cfg.K, 0.0) if cfg.option_type == 1 else max(cfg.K - final_S, 0.0)
            elif cfg.is_asian:
                avg_price = price_sum / steps if steps > 0 else final_S
                return max(avg_price - cfg.K, 0.0) if cfg.option_type == 1 else max(cfg.K - avg_price, 0.0)
            else:
                return max(final_S - cfg.K, 0.0) if cfg.option_type == 1 else max(cfg.K - final_S, 0.0)

        payoffs = []
        hits_zero = 0

        if cfg.antithetic:
            pairs = max(NSIM // 2, 1)
            for _ in range(pairs):
                S1 = cfg.S0
                S2 = cfg.S0
                sum1 = 0.0
                sum2 = 0.0
                ko1 = False
                ko2 = False
                hit1 = False
                hit2 = False

                for step in range(NT):
                    dW = random.gauss(0.0, 1.0)
                    t = step * k

                    if not ko1:
                        S1 = step_path(S1, dW, t, cfg.model_type, cfg.scheme)
                        if S1 <= 0.0:
                            hit1 = True
                        if cfg.is_barrier and cfg.is_up_and_out and S1 >= cfg.barrier:
                            ko1 = True
                        sum1 += S1

                    if not ko2:
                        S2 = step_path(S2, -dW, t, cfg.model_type, cfg.scheme)
                        if S2 <= 0.0:
                            hit2 = True
                        if cfg.is_barrier and cfg.is_up_and_out and S2 >= cfg.barrier:
                            ko2 = True
                        sum2 += S2

                    if ko1 and ko2:
                        break

                p1 = compute_payoff(S1, sum1, NT, ko1)
                p2 = compute_payoff(S2, sum2, NT, ko2)
                payoffs.append(0.5 * (p1 + p2))
                if hit1: hits_zero += 1
                if hit2: hits_zero += 1

            sample_count = pairs
        else:
            for _ in range(NSIM):
                S = cfg.S0
                price_sum = 0.0
                knocked_out = False
                path_hit_zero = False

                for step in range(NT):
                    dW = random.gauss(0.0, 1.0)
                    t = step * k
                    S = step_path(S, dW, t, cfg.model_type, cfg.scheme)
                    if S <= 0.0:
                        path_hit_zero = True
                    if cfg.is_barrier and cfg.is_up_and_out and S >= cfg.barrier:
                        knocked_out = True
                        break
                    price_sum += S

                if path_hit_zero:
                    hits_zero += 1

                payoffs.append(compute_payoff(S, price_sum, NT, knocked_out))
            sample_count = NSIM

        discount = math.exp(-r * T)
        mean_payoff = sum(payoffs) / sample_count
        price = discount * mean_payoff

        variance = sum((p - mean_payoff) ** 2 for p in payoffs) / (sample_count - 1) if sample_count > 1 else 0.0
        std_dev = math.sqrt(variance)
        std_err = std_dev / math.sqrt(sample_count)

        elapsed = time.perf_counter() - start_time

        return {
            "price": price,
            "std_dev": std_dev,
            "std_err": std_err,
            "hits_zero": hits_zero,
            "elapsed": elapsed,
            "sample_count": sample_count
        }

class TUI:
    def __init__(self):
        self.cfg = Config()
        self.last_res = None

    def draw_banner(self):
        print(f"{Color.CYAN}{Color.BOLD}" + "=" * 80)
        print("     MONTE CARLO OPTION PRICING SIMULATOR - INTERACTIVE TUI (PYTHON/C++)")
        print("=" * 80 + f"{Color.RESET}")

    def draw_dashboard(self):
        cfg = self.cfg
        print(f"{Color.BOLD}{Color.YELLOW}  [ACTIVE CONFIGURATION]{Color.RESET}")
        print(f"  * {Color.BOLD}Asset Model:       {Color.RESET}{Color.GREEN}{cfg.model_type}{Color.RESET}", end="")
        if cfg.model_type == "CEV":
            print(f"{Color.GRAY} (Elasticity beta = {cfg.cev_beta:.2f}){Color.RESET}")
        elif cfg.model_type == "CIR":
            print(f"{Color.GRAY} (kappa = {cfg.cir_kappa:.2f}, theta = {cfg.cir_theta:.2f}){Color.RESET}")
        else:
            print()

        print(f"  * {Color.BOLD}Contract Type:     {Color.RESET}{Color.GREEN}{cfg.contract_label}{Color.RESET}", end="")
        if cfg.is_barrier:
            print(f"{Color.GRAY} (Barrier = {cfg.barrier:.2f}, Up-and-Out){Color.RESET}")
        else:
            print()

        print(f"  * {Color.BOLD}Market Params:     {Color.RESET}S0 = {Color.CYAN}{cfg.S0:.4f}{Color.RESET}  |  K = {Color.CYAN}{cfg.K:.4f}{Color.RESET}  |  T = {Color.CYAN}{cfg.T:.4f} yrs{Color.RESET}")
        print(f"                       r = {Color.CYAN}{cfg.r:.4f}{Color.RESET}   |  sig = {Color.CYAN}{cfg.sig:.4f}{Color.RESET} |  D = {Color.CYAN}{cfg.D:.4f}{Color.RESET}")

        scheme_desc = {
            "Euler": "Euler-Maruyama (Strong Order 0.5)",
            "Milstein": "Milstein Scheme (Strong Order 1.0, diffusion derivative)",
            "PredictorCorrector": "Drift-Adjusted Predictor-Corrector (Two-step stabilized)"
        }[cfg.scheme]
        print(f"  * {Color.BOLD}Numerical Scheme:   {Color.RESET}{Color.MAGENTA}{scheme_desc}{Color.RESET}")
        print(f"  * {Color.BOLD}Simulation Paths:   {Color.RESET}NSIM = {Color.CYAN}{cfg.NSIM:,}{Color.RESET}  |  NT = {Color.CYAN}{cfg.NT}{Color.RESET} steps")
        print(f"  * {Color.BOLD}RNG & Variates:    {Color.RESET}Mersenne Twister (mt19937) (" + 
              (f"Fixed Seed = {cfg.seed_value}" if cfg.fixed_seed else "Hardware High-Res Time") + ")" + 
              (f"{Color.GREEN} [Antithetic Variates ON]{Color.RESET}" if cfg.antithetic else f"{Color.GRAY} [Standard Random]{Color.RESET}"))

        print(f"{Color.GRAY}" + "-" * 80 + f"{Color.RESET}")

        if self.last_res:
            self.draw_results_card(self.last_res)
            print(f"{Color.GRAY}" + "-" * 80 + f"{Color.RESET}")

        print(f"{Color.BOLD}{Color.YELLOW}  [INTERACTIVE MENU]{Color.RESET}")
        print(f"   {Color.BOLD}1.{Color.RESET} Run Pricing Simulation with Current Settings")
        print(f"   {Color.BOLD}2.{Color.RESET} Select Asset Dynamics Model (GBM / CEV / CIR)")
        print(f"   {Color.BOLD}3.{Color.RESET} Select Option Contract & Payoff Type")
        print(f"   {Color.BOLD}4.{Color.RESET} Edit Market & Contract Parameters (S0, K, T, r, sig, D, Barrier)")
        print(f"   {Color.BOLD}5.{Color.RESET} Select Finite Difference Scheme (Euler / Milstein / Pred-Corr)")
        print(f"   {Color.BOLD}6.{Color.RESET} Configure Simulation Settings (Paths, Steps, Seed, Antithetic)")
        print(f"   {Color.BOLD}7.{Color.RESET} Load Standard Market Scenario Presets")
        print(f"   {Color.BOLD}8.{Color.RESET} Compare All Numerical Schemes Side-by-Side")
        print(f"   {Color.BOLD}9.{Color.RESET} Export Benchmark Results to CSV")
        print(f"   {Color.BOLD}0.{Color.RESET} Exit Workbench")
        print(f"{Color.CYAN}" + "=" * 80 + f"{Color.RESET}")

    def draw_results_card(self, res: dict):
        cfg = self.cfg
        price = res["price"]
        se = res["std_err"]
        ci95_l = price - 1.95996 * se
        ci95_u = price + 1.95996 * se
        ci99_l = price - 2.57583 * se
        ci99_u = price + 2.57583 * se
        ms = res["elapsed"] * 1000.0
        throughput = cfg.NSIM / res["elapsed"] if res["elapsed"] > 0 else 0

        print(f"{Color.BOLD}{Color.GREEN}  [LATEST SIMULATION RESULTS]{Color.RESET}")
        print(f"   * {Color.BOLD}Monte Carlo Price:       {Color.RESET}{Color.GREEN}{Color.BOLD}{price:.5f}{Color.RESET}")
        print(f"   * {Color.BOLD}Standard Error (SE):     {Color.RESET}{se:.5f}")
        print(f"   * {Color.BOLD}95% Confidence Interval: {Color.RESET}[{ci95_l:.5f}, {ci95_u:.5f}]")
        print(f"   * {Color.BOLD}99% Confidence Interval: {Color.RESET}[{ci99_l:.5f}, {ci99_u:.5f}]")
        print(f"   * {Color.BOLD}Sample Standard Dev:     {Color.RESET}{res['std_dev']:.5f}")
        print(f"   * {Color.BOLD}Boundary Hits (S <= 0):  {Color.RESET}{res['hits_zero']:,} / {cfg.NSIM:,}")
        print(f"   * {Color.BOLD}Runtime & Throughput:    {Color.RESET}{ms:.2f} ms ({res['elapsed']:.3f} s) | {throughput:,.0f} paths/sec")

        has_analytical = (not cfg.is_barrier and not cfg.is_asian and cfg.model_type == "GBM")
        if has_analytical:
            bs_price = black_scholes_price(cfg.S0, cfg.K, cfg.T, cfg.r, cfg.sig, cfg.D, cfg.option_type)
            abs_err = abs(price - bs_price)
            rel_err = (abs_err / bs_price * 100.0) if bs_price != 0 else 0.0
            in_95 = (abs_err <= 1.95996 * se)
            cov_str = (f"{Color.GREEN}PASSED (Within 95% Confidence Interval){Color.RESET}" if in_95 else
                       f"{Color.YELLOW}Slight deviation outside 95% CI (Normal variance){Color.RESET}")

            print(f"   * {Color.BOLD}Black-Scholes Exact:     {Color.RESET}{Color.CYAN}{bs_price:.5f}{Color.RESET}")
            print(f"   * {Color.BOLD}Absolute Error |MC - BS|:{Color.RESET} {abs_err:.5f} ({rel_err:.2f}%)")
            print(f"   * {Color.BOLD}Analytical Coverage:     {Color.RESET}{cov_str}")

    def prompt_float(self, label: str, current: float, min_val: float = 0.0) -> float:
        while True:
            val_str = input(f"  {label} [{Color.CYAN}{current}{Color.RESET}]: ").strip()
            if not val_str:
                return current
            try:
                val = float(val_str)
                if val >= min_val:
                    return val
                print(f"{Color.RED}  Error: Value must be >= {min_val}. Try again.{Color.RESET}")
            except ValueError:
                print(f"{Color.RED}  Error: Invalid number format. Try again.{Color.RESET}")

    def prompt_int(self, label: str, current: int, min_val: int = 1) -> int:
        while True:
            val_str = input(f"  {label} [{Color.CYAN}{current}{Color.RESET}]: ").strip()
            if not val_str:
                return current
            try:
                val = int(val_str)
                if val >= min_val:
                    return val
                print(f"{Color.RED}  Error: Value must be >= {min_val}. Try again.{Color.RESET}")
            except ValueError:
                print(f"{Color.RED}  Error: Invalid integer format. Try again.{Color.RESET}")

    def pause(self):
        input(f"{Color.DIM}\nPress Enter to continue...{Color.RESET}")

    def menu_select_model(self):
        print(f"\n{Color.BOLD}{Color.YELLOW}--- Select Asset Dynamics Model ---{Color.RESET}")
        print("  1. Geometric Brownian Motion (GBM) [Black-Scholes standard]")
        print("  2. Constant Elasticity of Variance (CEV) [Leveraged elasticity]")
        print("  3. Cox-Ingersoll-Ross (CIR) [Mean-reverting square-root dynamics]")
        print("  0. Cancel")
        c = input("Choice [1-3, 0]: ").strip()
        if c == "1":
            self.cfg.model_type = "GBM"
            print(f"{Color.GREEN}Model set to GBM.{Color.RESET}")
        elif c == "2":
            self.cfg.model_type = "CEV"
            self.cfg.cev_beta = self.prompt_float("Enter CEV elasticity beta (e.g. 0.8)", self.cfg.cev_beta, 0.01)
            print(f"{Color.GREEN}Model set to CEV (beta = {self.cfg.cev_beta}).{Color.RESET}")
        elif c == "3":
            self.cfg.model_type = "CIR"
            self.cfg.cir_kappa = self.prompt_float("Enter CIR mean-reversion speed kappa", self.cfg.cir_kappa, 0.001)
            self.cfg.cir_theta = self.prompt_float("Enter CIR long-term mean theta", self.cfg.cir_theta, 0.001)
            print(f"{Color.GREEN}Model set to CIR (kappa = {self.cfg.cir_kappa}, theta = {self.cfg.cir_theta}).{Color.RESET}")

    def menu_select_contract(self):
        print(f"\n{Color.BOLD}{Color.YELLOW}--- Select Option Contract Type ---{Color.RESET}")
        print("  1. European Plain Vanilla Call")
        print("  2. European Plain Vanilla Put")
        print("  3. Up-and-Out Barrier Call")
        print("  4. Arithmetic Asian Call (Average Price)")
        print("  5. Arithmetic Asian Put (Average Price)")
        print("  0. Cancel")
        c = input("Choice [1-5, 0]: ").strip()
        if c == "1":
            self.cfg.contract_label = "European Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = False
            self.cfg.is_asian = False
            print(f"{Color.GREEN}Contract set to European Call.{Color.RESET}")
        elif c == "2":
            self.cfg.contract_label = "European Put"
            self.cfg.option_type = -1
            self.cfg.is_barrier = False
            self.cfg.is_asian = False
            print(f"{Color.GREEN}Contract set to European Put.{Color.RESET}")
        elif c == "3":
            self.cfg.contract_label = "Up-and-Out Barrier Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = True
            self.cfg.is_up_and_out = True
            self.cfg.is_asian = False
            self.cfg.barrier = self.prompt_float("Enter Barrier Level (B)", self.cfg.barrier, self.cfg.S0)
            print(f"{Color.GREEN}Contract set to Up-and-Out Barrier Call (B = {self.cfg.barrier}).{Color.RESET}")
        elif c == "4":
            self.cfg.contract_label = "Asian Arithmetic Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = False
            self.cfg.is_asian = True
            print(f"{Color.GREEN}Contract set to Asian Arithmetic Call.{Color.RESET}")
        elif c == "5":
            self.cfg.contract_label = "Asian Arithmetic Put"
            self.cfg.option_type = -1
            self.cfg.is_barrier = False
            self.cfg.is_asian = True
            print(f"{Color.GREEN}Contract set to Asian Arithmetic Put.{Color.RESET}")

    def menu_edit_params(self):
        print(f"\n{Color.BOLD}{Color.YELLOW}--- Edit Market & Contract Parameters ---{Color.RESET}")
        print(f"{Color.DIM}Press Enter on any field to keep existing value.{Color.RESET}")
        self.cfg.S0 = self.prompt_float("Initial Spot Price (S0)", self.cfg.S0, 0.001)
        self.cfg.K = self.prompt_float("Strike Price (K)", self.cfg.K, 0.001)
        self.cfg.T = self.prompt_float("Maturity Expiry in Years (T)", self.cfg.T, 0.001)
        self.cfg.r = self.prompt_float("Risk-Free Rate (r)", self.cfg.r, -0.50)
        self.cfg.sig = self.prompt_float("Volatility sigma", self.cfg.sig, 0.0001)
        self.cfg.D = self.prompt_float("Dividend Yield (D)", self.cfg.D, 0.0)

        if self.cfg.is_barrier:
            self.cfg.barrier = self.prompt_float("Barrier Level", self.cfg.barrier, 0.001)
        if self.cfg.model_type == "CEV":
            self.cfg.cev_beta = self.prompt_float("CEV Elasticity beta", self.cfg.cev_beta, 0.01)
        elif self.cfg.model_type == "CIR":
            self.cfg.cir_kappa = self.prompt_float("CIR kappa", self.cfg.cir_kappa, 0.001)
            self.cfg.cir_theta = self.prompt_float("CIR theta", self.cfg.cir_theta, 0.001)
        print(f"{Color.GREEN}Parameters updated.{Color.RESET}")

    def menu_select_scheme(self):
        print(f"\n{Color.BOLD}{Color.YELLOW}--- Select Finite Difference Scheme ---{Color.RESET}")
        print("  1. Euler-Maruyama (Strong Order 0.5)")
        print("  2. Milstein Scheme (Strong Order 1.0, diffusion derivative)")
        print("  3. Drift-Adjusted Predictor-Corrector (Two-step stabilized)")
        print("  0. Cancel")
        c = input("Choice [1-3, 0]: ").strip()
        if c == "1":
            self.cfg.scheme = "Euler"
            print(f"{Color.GREEN}Scheme set to Euler-Maruyama.{Color.RESET}")
        elif c == "2":
            self.cfg.scheme = "Milstein"
            print(f"{Color.GREEN}Scheme set to Milstein.{Color.RESET}")
        elif c == "3":
            self.cfg.scheme = "PredictorCorrector"
            print(f"{Color.GREEN}Scheme set to Predictor-Corrector.{Color.RESET}")

    def menu_configure_sim(self):
        print(f"\n{Color.BOLD}{Color.YELLOW}--- Configure Simulation & RNG Settings ---{Color.RESET}")
        self.cfg.NSIM = self.prompt_int("Simulation Paths (NSIM)", self.cfg.NSIM, 100)
        self.cfg.NT = self.prompt_int("Time Steps (NT)", self.cfg.NT, 10)

        anti_input = input(f"  Enable Antithetic Variates? (y/n) [{'y' if self.cfg.antithetic else 'n'}]: ").strip().lower()
        if anti_input:
            self.cfg.antithetic = (anti_input in ['y', 'yes'])

        seed_input = input(f"  Seed Mode: (1 = Random Time, 2 = Fixed Seed) [{'2' if self.cfg.fixed_seed else '1'}]: ").strip()
        if seed_input == "2":
            self.cfg.fixed_seed = True
            self.cfg.seed_value = self.prompt_int("Enter Fixed Seed Value", self.cfg.seed_value, 1)
        elif seed_input == "1":
            self.cfg.fixed_seed = False

        print(f"{Color.GREEN}Simulation settings updated.{Color.RESET}")

    def menu_presets(self):
        print(f"\n{Color.BOLD}{Color.YELLOW}--- Load Standard Scenario Presets ---{Color.RESET}")
        print("  1. Benchmark At-the-Money European Call (S0=100, K=100, T=1.0, r=5%, sig=20%)")
        print("  2. Volatility Skew Test under CEV Model (beta=0.60, S0=90, K=95, sig=20%)")
        print("  3. Mean-Reverting Short Rate under CIR Model (kappa=1.2, theta=0.04, S0=0.03)")
        print("  4. Up-and-Out Barrier Knockout Stress Test (S0=100, K=100, Barrier=115)")
        print("  5. Path-Dependent Arithmetic Asian Average Call (S0=90, K=95, T=0.5)")
        print("  0. Cancel")
        c = input("Choice [1-5, 0]: ").strip()
        if c == "1":
            self.cfg.model_type = "GBM"
            self.cfg.contract_label = "European Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = False
            self.cfg.is_asian = False
            self.cfg.S0 = 100.0; self.cfg.K = 100.0; self.cfg.T = 1.0; self.cfg.r = 0.05; self.cfg.sig = 0.20; self.cfg.D = 0.0
            self.cfg.scheme = "Euler"
            print(f"{Color.GREEN}Loaded: Benchmark ATM European Call.{Color.RESET}")
        elif c == "2":
            self.cfg.model_type = "CEV"
            self.cfg.cev_beta = 0.60
            self.cfg.contract_label = "European Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = False
            self.cfg.is_asian = False
            self.cfg.S0 = 90.0; self.cfg.K = 95.0; self.cfg.T = 0.5; self.cfg.r = 0.03; self.cfg.sig = 0.20; self.cfg.D = 0.0
            self.cfg.scheme = "Milstein"
            print(f"{Color.GREEN}Loaded: Volatility Skew CEV Model with Milstein.{Color.RESET}")
        elif c == "3":
            self.cfg.model_type = "CIR"
            self.cfg.cir_kappa = 1.20
            self.cfg.cir_theta = 0.04
            self.cfg.contract_label = "European Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = False
            self.cfg.is_asian = False
            self.cfg.S0 = 0.03; self.cfg.K = 0.035; self.cfg.T = 1.0; self.cfg.r = 0.03; self.cfg.sig = 0.08; self.cfg.D = 0.0
            self.cfg.scheme = "PredictorCorrector"
            print(f"{Color.GREEN}Loaded: Mean-Reverting Short Rate CIR Model.{Color.RESET}")
        elif c == "4":
            self.cfg.model_type = "GBM"
            self.cfg.contract_label = "Up-and-Out Barrier Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = True
            self.cfg.is_up_and_out = True
            self.cfg.barrier = 115.0
            self.cfg.is_asian = False
            self.cfg.S0 = 100.0; self.cfg.K = 100.0; self.cfg.T = 0.5; self.cfg.r = 0.05; self.cfg.sig = 0.20; self.cfg.D = 0.0
            self.cfg.scheme = "Euler"
            print(f"{Color.GREEN}Loaded: Up-and-Out Barrier Knockout Stress Test.{Color.RESET}")
        elif c == "5":
            self.cfg.model_type = "GBM"
            self.cfg.contract_label = "Asian Arithmetic Call"
            self.cfg.option_type = 1
            self.cfg.is_barrier = False
            self.cfg.is_asian = True
            self.cfg.S0 = 90.0; self.cfg.K = 95.0; self.cfg.T = 0.5; self.cfg.r = 0.03; self.cfg.sig = 0.15; self.cfg.D = 0.0
            self.cfg.scheme = "Euler"
            print(f"{Color.GREEN}Loaded: Path-Dependent Arithmetic Asian Call.{Color.RESET}")

    def menu_compare_schemes(self):
        print(f"\n{Color.BOLD}{Color.YELLOW}=== Comparing Numerical Schemes Side-by-Side ==={Color.RESET}")
        print(f"{Color.GRAY}Simulating Euler, Milstein, and Predictor-Corrector with identical parameters...\n{Color.RESET}")

        schemes = ["Euler", "Milstein", "PredictorCorrector"]
        results = []
        for s in schemes:
            temp_cfg = Config()
            temp_cfg.__dict__.update(self.cfg.__dict__)
            temp_cfg.scheme = s
            results.append((s, Simulator.run_simulation(temp_cfg)))

        print(f"{Color.BOLD}{Color.CYAN}{'Scheme':<20} {'MC Price':<12} {'Std Error':<12} {'95% Confidence Int':<24} {'Hits (S<=0)':<12} {'Time (ms)':<10}{Color.RESET}")
        print("-" * 92)
        for name, r in results:
            price = r["price"]
            se = r["std_err"]
            ci = f"[{price - 1.96 * se:.4f}, {price + 1.96 * se:.4f}]"
            ms = r["elapsed"] * 1000.0
            print(f"{name:<20} {price:<12.4f} {se:<12.4f} {ci:<24} {r['hits_zero']:<12} {ms:<10.2f}")

        if not self.cfg.is_barrier and not self.cfg.is_asian and self.cfg.model_type == "GBM":
            bs = black_scholes_price(self.cfg.S0, self.cfg.K, self.cfg.T, self.cfg.r, self.cfg.sig, self.cfg.D, self.cfg.option_type)
            print("-" * 92)
            print(f"{Color.BOLD}{Color.GREEN}Exact Analytical Black-Scholes Price: {bs:.5f}{Color.RESET}")

        self.pause()

    def menu_export_csv(self):
        if not self.last_res:
            print(f"{Color.YELLOW}No simulation result to export. Run a simulation first!{Color.RESET}")
            self.pause()
            return
        filename = f"mc_result_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        with open(filename, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["Model", "Contract", "S0", "K", "T", "r", "sig", "D", "Scheme", "NSIM", "NT", "Price", "StdDev", "StdErr", "HitsZero", "ElapsedSeconds"])
            writer.writerow([
                self.cfg.model_type, self.cfg.contract_label, self.cfg.S0, self.cfg.K, self.cfg.T, self.cfg.r, self.cfg.sig, self.cfg.D,
                self.cfg.scheme, self.cfg.NSIM, self.cfg.NT,
                f"{self.last_res['price']:.6f}", f"{self.last_res['std_dev']:.6f}", f"{self.last_res['std_err']:.6f}",
                self.last_res['hits_zero'], f"{self.last_res['elapsed']:.6f}"
            ])
        print(f"{Color.GREEN}Result successfully exported to {filename}!{Color.RESET}")
        self.pause()

    def run(self):
        while True:
            self.draw_banner()
            self.draw_dashboard()
            choice = input("Enter option [0-9]: ").strip()

            if choice in ["0", "exit", "q"]:
                print("\nExiting Monte Carlo Pricing Workbench. Goodbye!")
                break
            elif choice == "1":
                print(f"\n{Color.BOLD}Simulating {self.cfg.NSIM:,} paths across {self.cfg.NT} steps...{Color.RESET}")
                self.last_res = Simulator.run_simulation(self.cfg)
                print(f"{Color.GREEN}Simulation Complete!{Color.RESET}")
                self.pause()
            elif choice == "2":
                self.menu_select_model()
            elif choice == "3":
                self.menu_select_contract()
            elif choice == "4":
                self.menu_edit_params()
            elif choice == "5":
                self.menu_select_scheme()
            elif choice == "6":
                self.menu_configure_sim()
            elif choice == "7":
                self.menu_presets()
            elif choice == "8":
                self.menu_compare_schemes()
            elif choice == "9":
                self.menu_export_csv()
            else:
                print(f"{Color.RED}Invalid option. Enter a number between 0 and 9.{Color.RESET}")

if __name__ == "__main__":
    tui = TUI()
    tui.run()
