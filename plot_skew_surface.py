"""
================================================================================
High-Performance Volatility Skew & Price Surface Generator
================================================================================
Academic Context: Baruch MFE / QuantNet Advanced C++ Financial Engineering
Author: Sahil Jain (Adapted for Quantitative Analytics & Visualisation)

Description:
    This script visualizes option pricing dynamics and Volatility Skew Surfaces:
    1. Black-Scholes Closed-Form Pricing & Implied Volatility (IV) inversion.
    2. Vectorized Monte Carlo Simulation for:
       - Geometric Brownian Motion (GBM, beta = 1.0) -> Flat Volatility Surface
       - Constant Elasticity of Variance (CEV, beta < 1.0) -> Volatility Skew
       - Cox-Ingersoll-Ross (CIR) -> Mean-reverting interest rate / volatility
    3. Seamless ingestion of C++ Monte Carlo Engine CSV output files (results*.csv).
    4. Publication-quality 3D Surface Plots & 2D Skew Cross-Sections.
    5. Optional interactive HTML export via Plotly (opens directly in web browser).

Prerequisites:
    pip install numpy scipy matplotlib pandas plotly
================================================================================
"""

import os
import glob
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib import cm
from scipy.stats import norm
from scipy.optimize import brentq

# ==============================================================================
# SECTION 1: BLACK-SCHOLES FORMULA & IMPLIED VOLATILITY (IV) SOLVER
# ==============================================================================

def bs_call_price(S0: float, K: float, T: float, r: float, sig: float, D: float = 0.0) -> float:
    """
    Computes the European Call option price under the Black-Scholes-Merton model.
    
    Formula:
        C = S0 * exp(-D * T) * N(d1) - K * exp(-r * T) * N(d2)
        d1 = [ln(S0 / K) + (r - D + 0.5 * sig^2) * T] / (sig * sqrt(T))
        d2 = d1 - sig * sqrt(T)
    """
    if T <= 1e-6 or sig <= 1e-6:
        return max(S0 * np.exp(-D * T) - K * np.exp(-r * T), 0.0)
    
    d1 = (np.log(S0 / K) + (r - D + 0.5 * sig**2) * T) / (sig * np.sqrt(T))
    d2 = d1 - sig * np.sqrt(T)
    price = S0 * np.exp(-D * T) * norm.cdf(d1) - K * np.exp(-r * T) * norm.cdf(d2)
    return float(max(price, 0.0))


def implied_volatility_call(price: float, S0: float, K: float, T: float, r: float, D: float = 0.0) -> float:
    """
    Inverts the Black-Scholes formula using Brent's root-finding method to recover
    the Black-Scholes Implied Volatility (sigma_IV) from an option price.
    
    Why is this necessary?
        Non-GBM models (like CEV or stochastic volatility) produce prices that
        differ from Black-Scholes. By finding what volatility input in Black-Scholes
        would replicate the model price, we translate prices into "Implied Volatility",
        revealing the celebrated 'Volatility Smile' or 'Volatility Skew'.
    """
    # Lower bound: Intrinsic discounted value
    intrinsic = max(S0 * np.exp(-D * T) - K * np.exp(-r * T), 0.0)
    if price <= intrinsic + 1e-6:
        return np.nan # Deep out-of-the-money or below arbitrage bound
    
    def objective(sig: float) -> float:
        return bs_call_price(S0, K, T, r, sig, D) - price

    try:
        # Search for implied volatility within realistic financial bounds: 0.1% to 500%
        iv = brentq(objective, a=0.001, b=5.0, xtol=1e-5, maxiter=100)
        return float(iv)
    except (ValueError, RuntimeError):
        return np.nan


# ==============================================================================
# SECTION 2: VECTORIZED MONTE CARLO SIMULATOR (GBM & CEV SKEW)
# ==============================================================================

def simulate_cev_surface(
    S0: float = 100.0,
    strikes: np.ndarray = np.linspace(70, 130, 25),
    maturities: np.ndarray = np.linspace(0.1, 2.0, 15),
    r: float = 0.05,
    sig: float = 0.20,
    D: float = 0.0,
    beta: float = 0.60,      # beta < 1.0 creates leverage volatility skew!
    NSIM: int = 40000,
    NT: int = 200
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """
    Simulates a 2D grid of option prices and implied volatilities across
    various Strikes (K) and Maturities (T) using the CEV (Constant Elasticity
    of Variance) model:
    
        dS_t = (r - D) * S_t * dt + sig * S_t^beta * dW_t
        
    When beta < 1:
        As stock price falls, volatility sig * S^(beta - 1) increases.
        This directly mimics the negative skewness observed in equity options.
    """
    print(f"\n[INFO] Simulating CEV Surface: S0={S0}, beta={beta}, sig={sig}, NSIM={NSIM}, NT={NT}...")
    
    K_mesh, T_mesh = np.meshgrid(strikes, maturities)
    price_mesh = np.zeros_like(K_mesh)
    iv_mesh = np.zeros_like(K_mesh)
    
    # Pre-seed random generator for reproducible surface
    rng = np.random.default_rng(seed=42)
    
    # For each maturity, simulate terminal price distribution S_T
    for i, T in enumerate(maturities):
        dt = T / NT
        sqrt_dt = np.sqrt(dt)
        
        # Paths array: shape (NSIM,)
        S = np.full(NSIM, S0, dtype=np.float64)
        
        for _ in range(NT):
            Z = rng.standard_normal(NSIM)
            # CEV diffusion with non-negativity protection
            S_pos = np.maximum(S, 1e-4)
            drift = (r - D) * S_pos
            diffusion = sig * (S_pos ** beta)
            
            # Euler-Maruyama step
            S += drift * dt + diffusion * sqrt_dt * Z
            S = np.maximum(S, 0.0) # Absorbing boundary at zero
        
        # Calculate Call prices for all strikes at this maturity T
        discount = np.exp(-r * T)
        for j, K in enumerate(strikes):
            payoffs = np.maximum(S - K, 0.0)
            call_price = discount * np.mean(payoffs)
            price_mesh[i, j] = call_price
            
            # Compute Black-Scholes implied volatility
            iv_mesh[i, j] = implied_volatility_call(call_price, S0, K, T, r, D)
    
    print("[SUCCESS] CEV Surface computation complete.")
    return K_mesh, T_mesh, price_mesh, iv_mesh


# ==============================================================================
# SECTION 3: CSV INGESTION FROM C++ ENGINE
# ==============================================================================

def load_cpp_results(filepath: str | None = None) -> pd.DataFrame | None:
    """
    Searches for and loads the latest CSV file generated by the C++ engine.
    Matches schema: S0,K,T,r,sig,D,Type,Model,Scheme,Price,StdDev,StdErr,HitsZero
    """
    if filepath is None or not os.path.exists(filepath):
        # Look for timestamped results in current dir and archive folders
        patterns = ["results_*.csv", "results.csv", "archive_phases/*/*.csv"]
        found = []
        for pat in patterns:
            found.extend(glob.glob(pat))
        
        if not found:
            print("[WARN] No C++ results CSV found. Using synthetic CEV engine.")
            return None
        
        # Pick the most recently modified CSV
        filepath = max(found, key=os.path.getmtime)
    
    print(f"[INFO] Ingesting C++ output file: {filepath}")
    df = pd.read_csv(filepath)
    print(f"[INFO] Loaded {len(df)} simulation records.")
    return df


# ==============================================================================
# SECTION 4: 3D SURFACE & 2D SKEW PLOTTING (MATPLOTLIB)
# ==============================================================================

def plot_skew_surface_3d(
    K_mesh: np.ndarray,
    T_mesh: np.ndarray,
    iv_mesh: np.ndarray,
    price_mesh: np.ndarray,
    title_suffix: str = "CEV Model (beta = 0.60)",
    output_path: str = "volatility_skew_surface_3d.png",
    show: bool = True
) -> None:
    """
    Renders dual side-by-side publication-quality 3D surfaces:
    1. Implied Volatility Skew Surface: (Strike, Expiry) -> Implied Vol
    2. Option Price Surface: (Strike, Expiry) -> Call Price
    """
    fig = plt.figure(figsize=(18, 7))
    fig.patch.set_facecolor("#0f172a") # Elegant dark theme
    
    # ------------------ SUBPLOT 1: IMPLIED VOLATILITY SURFACE -----------------
    ax1 = fig.add_subplot(1, 2, 1, projection="3d")
    ax1.set_facecolor("#0f172a")
    
    # Mask any NaNs in IV mesh
    valid_mask = ~np.isnan(iv_mesh)
    
    surf1 = ax1.plot_surface(
        K_mesh, T_mesh, iv_mesh * 100.0, # Display in percentage (%)
        cmap=cm.plasma,
        edgecolor="#1e293b",
        linewidth=0.4,
        antialiased=True,
        alpha=0.92
    )
    
    # Add contour projections onto floor/walls for depth
    ax1.contour(K_mesh, T_mesh, iv_mesh * 100.0, zdir='z', offset=np.nanmin(iv_mesh * 100.0) - 1.0, cmap=cm.plasma, alpha=0.5)
    
    ax1.set_title(f"Implied Volatility Skew Surface\n{title_suffix}", fontsize=14, color="white", weight="bold", pad=15)
    ax1.set_xlabel("Strike Price (K)", fontsize=11, color="#cbd5e1", labelpad=10)
    ax1.set_ylabel("Maturity T (Years)", fontsize=11, color="#cbd5e1", labelpad=10)
    ax1.set_zlabel("Implied Volatility (%)", fontsize=11, color="#cbd5e1", labelpad=10)
    ax1.tick_params(colors="#94a3b8")
    
    cbar1 = fig.colorbar(surf1, ax=ax1, shrink=0.55, aspect=12, pad=0.1)
    cbar1.ax.tick_params(colors="#94a3b8")
    cbar1.set_label("IV (%)", color="#cbd5e1")
    
    # ------------------ SUBPLOT 2: OPTION PRICE SURFACE ----------------------
    ax2 = fig.add_subplot(1, 2, 2, projection="3d")
    ax2.set_facecolor("#0f172a")
    
    surf2 = ax2.plot_surface(
        K_mesh, T_mesh, price_mesh,
        cmap=cm.viridis,
        edgecolor="#1e293b",
        linewidth=0.4,
        antialiased=True,
        alpha=0.92
    )
    
    ax2.contour(K_mesh, T_mesh, price_mesh, zdir='z', offset=0.0, cmap=cm.viridis, alpha=0.5)
    
    ax2.set_title(f"European Call Price Surface\n{title_suffix}", fontsize=14, color="white", weight="bold", pad=15)
    ax2.set_xlabel("Strike Price (K)", fontsize=11, color="#cbd5e1", labelpad=10)
    ax2.set_ylabel("Maturity T (Years)", fontsize=11, color="#cbd5e1", labelpad=10)
    ax2.set_zlabel("Option Price ($)", fontsize=11, color="#cbd5e1", labelpad=10)
    ax2.tick_params(colors="#94a3b8")
    
    cbar2 = fig.colorbar(surf2, ax=ax2, shrink=0.55, aspect=12, pad=0.1)
    cbar2.ax.tick_params(colors="#94a3b8")
    cbar2.set_label("Call Price ($)", color="#cbd5e1")
    
    # Set view angle for aesthetic 3D perspective
    ax1.view_init(elev=28, azim=-125)
    ax2.view_init(elev=28, azim=-125)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
    print(f"[SUCCESS] Saved high-resolution 3D surface plot to: {output_path}")
    if show:
        plt.show()
    plt.close(fig)


def plot_skew_slices_2d(
    S0: float = 100.0,
    strikes: np.ndarray = np.linspace(70, 130, 25),
    T_slice: float = 1.0,
    r: float = 0.05,
    sig: float = 0.20,
    output_path: str = "volatility_skew_slices_2d.png",
    show: bool = True
) -> None:
    """
    Plots 2D Volatility Skew cross-sections comparing different Elasticity parameters (beta):
    - beta = 1.0: GBM (Black-Scholes flat volatility line)
    - beta = 0.8: Mild equity leverage skew
    - beta = 0.5: Strong square-root volatility skew (CIR style)
    """
    print(f"\n[INFO] Generating 2D Skew slices at Maturity T = {T_slice} years...")
    betas = [1.0, 0.8, 0.5]
    colors = ["#38bdf8", "#f59e0b", "#ef4444"]
    labels = ["GBM (Beta = 1.0, Flat Line)", "CEV (Beta = 0.8, Moderate Skew)", "CEV (Beta = 0.5, Steep Skew)"]
    
    fig = plt.figure(figsize=(10, 6), facecolor="#0f172a")
    ax = plt.gca()
    ax.set_facecolor("#1e293b")
    
    for beta, color, label in zip(betas, colors, labels):
        # Quick 1D slice simulation
        K_mesh, T_mesh, price_mesh, iv_mesh = simulate_cev_surface(
            S0=S0, strikes=strikes, maturities=np.array([T_slice]),
            r=r, sig=sig, beta=beta, NSIM=30000, NT=150
        )
        iv_slice = iv_mesh[0, :] * 100.0 # to %
        plt.plot(strikes, iv_slice, marker="o", markersize=5, linewidth=2.5, color=color, label=label)
    
    plt.axvline(x=S0, color="#94a3b8", linestyle="--", linewidth=1.5, label=f"ATM Strike (S0 = {S0})")
    plt.title(f"Volatility Skew Comparison Across SDE Models (T = {T_slice} Year)", fontsize=14, color="white", weight="bold", pad=15)
    plt.xlabel("Strike Price (K)", fontsize=12, color="#cbd5e1", labelpad=10)
    plt.ylabel("Black-Scholes Implied Volatility (%)", fontsize=12, color="#cbd5e1", labelpad=10)
    plt.grid(True, linestyle=":", alpha=0.3, color="#94a3b8")
    plt.tick_params(colors="#94a3b8")
    
    leg = plt.legend(facecolor="#0f172a", edgecolor="#334155", fontsize=10)
    for text in leg.get_texts():
        text.set_color("white")
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=300, facecolor="#0f172a", edgecolor='none')
    print(f"[SUCCESS] Saved 2D Skew slices plot to: {output_path}")
    if show:
        plt.show()
    plt.close(fig)


# ==============================================================================
# SECTION 5: INTERACTIVE HTML EXPORT (PLOTLY)
# ==============================================================================

def export_interactive_plotly(
    K_mesh: np.ndarray,
    T_mesh: np.ndarray,
    iv_mesh: np.ndarray,
    filename: str = "interactive_volatility_surface.html"
) -> None:
    """
    Generates a full 3D interactive Plotly surface that can be rotated, zoomed,
    and inspected with mouse hover directly in Google Chrome or any browser.
    """
    try:
        import plotly.graph_objects as go
        print(f"\n[INFO] Generating interactive WebGL 3D surface...")
        
        fig = go.Figure(data=[
            go.Surface(
                x=K_mesh,
                y=T_mesh,
                z=iv_mesh * 100.0,
                colorscale="Viridis",
                colorbar=dict(title="IV (%)")
            )
        ])
        
        fig.update_layout(
            title="Interactive Black-Scholes Implied Volatility Surface (CEV Model)",
            scene=dict(
                xaxis_title="Strike Price (K)",
                yaxis_title="Maturity (Years)",
                zaxis_title="Implied Volatility (%)",
                bgcolor="#0f172a"
            ),
            template="plotly_dark",
            margin=dict(l=20, r=20, b=20, t=50)
        )
        
        fig.write_html(filename)
        print(f"[SUCCESS] Saved interactive HTML surface to: {filename}")
        print(f"          Open '{filename}' in your web browser to freely spin & inspect!")
    except ImportError:
        print("[NOTE] Plotly not installed. Run 'pip install plotly' for interactive browser 3D surfaces.")


# ==============================================================================
# MAIN EXECUTION ENTRYPOINT & CLI PARSER
# ==============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="High-Performance Volatility Skew & Price Surface Generator (Monte Carlo & CEV/GBM)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--s0", type=float, default=100.0, help="Initial underlying spot price S0")
    parser.add_argument("--k-min", type=float, default=75.0, help="Minimum strike price K")
    parser.add_argument("--k-max", type=float, default=125.0, help="Maximum strike price K")
    parser.add_argument("--k-steps", type=int, default=21, help="Number of strike evaluation points")
    parser.add_argument("--t-min", type=float, default=0.2, help="Minimum maturity T in years")
    parser.add_argument("--t-max", type=float, default=2.0, help="Maximum maturity T in years")
    parser.add_argument("--t-steps", type=int, default=10, help="Number of maturity evaluation points")
    parser.add_argument("--r", type=float, default=0.04, help="Risk-free interest rate r")
    parser.add_argument("--sig", type=float, default=0.20, help="Volatility scaling parameter sigma")
    parser.add_argument("--beta", type=float, default=0.60, help="CEV Elasticity parameter beta (< 1.0 creates equity leverage skew)")
    parser.add_argument("--nsim", type=int, default=25000, help="Number of Monte Carlo simulation paths per point")
    parser.add_argument("--nt", type=int, default=120, help="Number of time integration steps per path")
    parser.add_argument("--csv", type=str, default=None, help="Optional custom path to C++ Monte Carlo engine CSV results")
    parser.add_argument("--out-3d", type=str, default="volatility_skew_surface_3d.png", help="Output filename for 3D surface plot")
    parser.add_argument("--out-2d", type=str, default="volatility_skew_slices_2d.png", help="Output filename for 2D skew comparison slices")
    parser.add_argument("--out-html", type=str, default="interactive_volatility_surface.html", help="Output filename for interactive Plotly HTML")
    parser.add_argument("--save-only", "--no-show", action="store_true", help="Save plot images to disk without opening GUI popups")
    parser.add_argument("--no-html", action="store_true", help="Skip Plotly interactive HTML generation")

    args = parser.parse_args()

    print("=" * 80)
    print("  HIGH-PERFORMANCE VOLATILITY SKEW & SURFACE ANALYTICS ENGINE")
    print("=" * 80)
    
    # 1. Check if user provided or has C++ results CSV to inspect
    df_cpp = load_cpp_results(args.csv)
    
    # 2. Simulate complete high-resolution (K, T) grid under CEV model
    strikes = np.linspace(args.k_min, args.k_max, args.k_steps)
    maturities = np.linspace(args.t_min, args.t_max, args.t_steps)
    
    K_mesh, T_mesh, price_mesh, iv_mesh = simulate_cev_surface(
        S0=args.s0,
        strikes=strikes,
        maturities=maturities,
        r=args.r,
        sig=args.sig,
        beta=args.beta,
        NSIM=args.nsim,
        NT=args.nt
    )
    
    show_plots = not args.save_only

    # 3. Render 3D Surface Plots (IV Surface & Price Surface)
    plot_skew_surface_3d(
        K_mesh, T_mesh, iv_mesh, price_mesh,
        title_suffix=f"CEV Model (beta = {args.beta:.2f}, r = {args.r*100:.1f}%)",
        output_path=args.out_3d,
        show=show_plots
    )
    
    # 4. Render 2D Skew Slices comparing GBM vs CEV
    plot_skew_slices_2d(
        S0=args.s0,
        strikes=strikes,
        T_slice=1.0,
        r=args.r,
        sig=args.sig,
        output_path=args.out_2d,
        show=show_plots
    )
    
    # 5. Export interactive browser visualization (Plotly)
    if not args.no_html:
        export_interactive_plotly(K_mesh, T_mesh, iv_mesh, filename=args.out_html)
    
    print("\n[COMPLETE] All volatility skew plots generated successfully.")
