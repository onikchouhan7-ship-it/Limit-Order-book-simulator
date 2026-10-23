from __future__ import annotations

import json
import subprocess
from pathlib import Path

import pandas as pd
import plotly.graph_objects as go
import streamlit as st

ROOT = Path(__file__).resolve().parent
BRIDGE_EXE = ROOT / "bridge.exe"

st.set_page_config(
    page_title="Limit Order Book Simulator",
    page_icon="📈",
    layout="wide",
    initial_sidebar_state="expanded",
)

st.markdown(
    """
    <style>
    .block-container {padding-top: 1.2rem; padding-bottom: 2rem; max-width: 1600px;}
    [data-testid="stMetric"] {background: rgba(120,120,120,.08); padding: 14px; border-radius: 12px; border: 1px solid rgba(128,128,128,.16);}
    div[data-testid="stForm"] {border: 1px solid rgba(128,128,128,.25); padding: 14px; border-radius: 12px;}
    </style>
    """,
    unsafe_allow_html=True,
)


class CppBridge:
    """One persistent C++ process; the engine state survives Streamlit reruns."""

    def __init__(self) -> None:
        if not BRIDGE_EXE.exists():
            raise FileNotFoundError(
                f"Could not find {BRIDGE_EXE.name}. Compile bridge.cpp first with: "
                "g++ -std=c++17 -O2 bridge.cpp -o bridge.exe"
            )
        self.process = subprocess.Popen(
            [str(BRIDGE_EXE)],
            cwd=str(ROOT),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def send(self, command: str) -> dict:
        if self.process.poll() is not None:
            raise RuntimeError("The C++ bridge stopped. Stop and restart Streamlit.")
        if self.process.stdin is None or self.process.stdout is None:
            raise RuntimeError("Could not open communication pipes to the C++ bridge.")
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        response = self.process.stdout.readline()
        if not response:
            raise RuntimeError("The C++ bridge returned no response. Recompile bridge.cpp and restart the app.")
        try:
            return json.loads(response)
        except json.JSONDecodeError as exc:
            raise RuntimeError(f"Invalid response from C++ bridge: {response[:200]}") from exc


@st.cache_resource
def get_bridge() -> CppBridge:
    return CppBridge()


try:
    bridge = get_bridge()
except Exception as exc:
    st.error(str(exc))
    st.info("Compile bridge.cpp from the project folder, then restart Streamlit.")
    st.code("g++ -std=c++17 -O2 bridge.cpp -o bridge.exe\npython -m streamlit run app.py", language="powershell")
    st.stop()


def show_feedback(result: dict) -> None:
    if result.get("ok"):
        st.success(result.get("message", "Done"))
    else:
        st.error(result.get("message", "The operation failed"))


st.title("📈 Limit Order Book Simulator")
st.caption("C++ matching engine  ·  Price-time priority  ·  Live order book  ·  Market-depth and execution analytics")

with st.sidebar:
    st.header("Order controls")
    st.caption("All prices and quantities are simulated. No exchange or live market data is connected.")

    with st.form("limit_order_form", clear_on_submit=True):
        st.subheader("Limit order")
        limit_id = st.number_input("Order ID", min_value=1, step=1, key="limit_id")
        limit_side = st.selectbox("Side", ["BUY", "SELL"], key="limit_side")
        limit_price = st.number_input("Limit price", min_value=1, step=1, value=100, key="limit_price")
        limit_qty = st.number_input("Quantity", min_value=1, step=1, value=10, key="limit_qty")
        limit_submit = st.form_submit_button("Submit limit order", use_container_width=True)

    if limit_submit:
        show_feedback(bridge.send(f"ADD {int(limit_id)} {limit_side} {int(limit_price)} {int(limit_qty)}"))

    with st.form("market_order_form", clear_on_submit=True):
        st.subheader("Market order")
        market_id = st.number_input("Market order ID", min_value=1, step=1, key="market_id")
        market_side = st.selectbox("Market side", ["BUY", "SELL"], key="market_side")
        market_qty = st.number_input("Market quantity", min_value=1, step=1, value=5, key="market_qty")
        market_submit = st.form_submit_button("Submit market order", use_container_width=True)

    if market_submit:
        show_feedback(bridge.send(f"MARKET {int(market_id)} {market_side} {int(market_qty)}"))

    with st.form("cancel_form", clear_on_submit=True):
        st.subheader("Cancel resting order")
        cancel_id = st.number_input("Order ID to cancel", min_value=1, step=1, key="cancel_id")
        cancel_submit = st.form_submit_button("Cancel order", use_container_width=True)

    if cancel_submit:
        show_feedback(bridge.send(f"CANCEL {int(cancel_id)}"))

    st.divider()
    if st.button("Reset simulator", type="secondary", use_container_width=True):
        show_feedback(bridge.send("RESET"))

    st.caption("Dashboard data refreshes automatically every 2 seconds.")


def make_depth_figure(bids: list[dict], asks: list[dict]) -> go.Figure:
    fig = go.Figure()

    if bids:
        # Plot lower bid prices first; cumulative bid depth falls toward the best bid.
        bid_levels = sorted(bids, key=lambda item: int(item["price"]))
        cumulative_by_price: dict[int, int] = {}
        running = 0
        for level in reversed(bid_levels):
            running += int(level["quantity"])
            cumulative_by_price[int(level["price"])] = running
        fig.add_trace(go.Scatter(
            x=[int(level["price"]) for level in bid_levels],
            y=[cumulative_by_price[int(level["price"])] for level in bid_levels],
            mode="lines+markers", name="Bid depth", fill="tozeroy", line_shape="hv",
            hovertemplate="Bid price: %{x}<br>Cumulative quantity: %{y}<extra></extra>",
        ))

    if asks:
        ask_levels = sorted(asks, key=lambda item: int(item["price"]))
        running = 0
        ask_cumulative = []
        for level in ask_levels:
            running += int(level["quantity"])
            ask_cumulative.append(running)
        fig.add_trace(go.Scatter(
            x=[int(level["price"]) for level in ask_levels],
            y=ask_cumulative,
            mode="lines+markers", name="Ask depth", fill="tozeroy", line_shape="hv",
            hovertemplate="Ask price: %{x}<br>Cumulative quantity: %{y}<extra></extra>",
        ))

    fig.update_layout(
        xaxis_title="Price", yaxis_title="Cumulative quantity", height=370,
        margin=dict(l=10, r=10, t=20, b=10), hovermode="x unified",
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="left", x=0),
    )
    return fig


def make_trade_figure(trades: list[dict]) -> go.Figure:
    fig = go.Figure()
    if trades:
        recent = trades[-50:]
        fig.add_trace(go.Scatter(
            x=list(range(1, len(recent) + 1)),
            y=[int(trade["price"]) for trade in recent],
            mode="lines+markers", name="Execution price",
            customdata=[int(trade["quantity"]) for trade in recent],
            hovertemplate="Trade %{x}<br>Price: %{y}<br>Quantity: %{customdata}<extra></extra>",
        ))
    fig.update_layout(
        xaxis_title="Recent executions (oldest → newest)", yaxis_title="Execution price",
        height=300, margin=dict(l=10, r=10, t=20, b=10), showlegend=False,
    )
    return fig


@st.fragment(run_every="2s")
def render_dashboard() -> None:
    try:
        state = bridge.send("STATE")
    except Exception as exc:
        st.error(f"Could not refresh the order book: {exc}")
        return

    bids = state.get("bids", [])
    asks = state.get("asks", [])
    trades = state.get("trades", [])
    active = state.get("active_orders", [])

    best_bid = state.get("best_bid")
    best_ask = state.get("best_ask")
    spread = state.get("spread")
    volume = int(state.get("traded_volume", 0))
    notional = int(state.get("traded_notional", 0))
    trade_count = int(state.get("trade_count", 0))
    average_price = (notional / volume) if volume else None
    latest_trade = trades[-1] if trades else None

    total_bid_qty = sum(int(level["quantity"]) for level in bids)
    total_ask_qty = sum(int(level["quantity"]) for level in asks)
    total_depth = total_bid_qty + total_ask_qty
    imbalance = ((total_bid_qty - total_ask_qty) / total_depth * 100) if total_depth else None

    top = st.columns(5)
    top[0].metric("Best bid", "—" if best_bid is None else f"{int(best_bid):,}")
    top[1].metric("Best ask", "—" if best_ask is None else f"{int(best_ask):,}")
    top[2].metric("Bid–ask spread", "—" if spread is None else f"{int(spread):,}")
    top[3].metric("Trade count", f"{trade_count:,}")
    top[4].metric("Traded quantity", f"{volume:,}")

    stats = st.columns(4)
    stats[0].metric("Weighted avg. execution", "—" if average_price is None else f"{average_price:,.2f}")
    stats[1].metric("Latest execution", "—" if latest_trade is None else f"{int(latest_trade['price']):,}")
    stats[2].metric("Traded notional", f"{notional:,}")
    stats[3].metric("Book imbalance", "—" if imbalance is None else f"{imbalance:+.1f}%", help="Positive means more displayed bid quantity; negative means more displayed ask quantity.")

    left, right = st.columns([1, 1], gap="large")
    with left:
        st.subheader("Order book")
        bid_col, ask_col = st.columns(2)
        with bid_col:
            st.markdown("**Bids · highest price first**")
            if bids:
                bid_df = pd.DataFrame(bids)[["price", "quantity", "orders"]]
                bid_df.columns = ["Price", "Quantity", "Orders"]
                st.dataframe(bid_df, hide_index=True, use_container_width=True, height=280)
            else:
                st.info("No buy orders on the book.")
        with ask_col:
            st.markdown("**Asks · lowest price first**")
            if asks:
                ask_df = pd.DataFrame(asks)[["price", "quantity", "orders"]]
                ask_df.columns = ["Price", "Quantity", "Orders"]
                st.dataframe(ask_df, hide_index=True, use_container_width=True, height=280)
            else:
                st.info("No sell orders on the book.")

        st.subheader("Active resting orders")
        if active:
            active_df = pd.DataFrame(active)
            active_df.columns = ["Order ID", "Side", "Price", "Remaining quantity"]
            st.dataframe(active_df, hide_index=True, use_container_width=True, height=240)
        else:
            st.caption("No active resting orders.")

    with right:
        st.subheader("Cumulative market depth")
        if bids or asks:
            st.plotly_chart(make_depth_figure(bids, asks), use_container_width=True)
        else:
            st.info("Add resting limit orders to populate market depth.")

        st.subheader("Execution price history")
        if trades:
            st.plotly_chart(make_trade_figure(trades), use_container_width=True)
        else:
            st.caption("Execution prices appear after orders trade.")

    st.subheader("Recent executions")
    if trades:
        recent = list(reversed(trades[-20:]))
        trade_df = pd.DataFrame(recent)[["buy_id", "sell_id", "price", "quantity"]]
        trade_df.columns = ["Buy order ID", "Sell order ID", "Execution price", "Quantity"]
        st.dataframe(trade_df, hide_index=True, use_container_width=True, height=240)
    else:
        st.caption("No trades yet. Submit crossing limit orders or a market order.")

    st.caption("Refreshes every 2 seconds · Educational simulation only · No exchange or live market data")


render_dashboard()
