<<<<<<< HEAD
# C++ Limit Order Book Simulator

A limit order book simulator built with **C++ and Python**, featuring price-time priority matching, partial order fills, market and limit orders, trade analytics, and an interactive Streamlit dashboard.

## Overview

This project simulates the core mechanics of an electronic exchange. Buy and sell orders are maintained at different price levels, and compatible orders are matched according to price priority and arrival order.

A C++ engine manages the order book and matching logic, while a Python dashboard provides a visual interface for submitting orders, monitoring market depth, and analyzing executions.

## Features

- **Limit orders:** Submit buy and sell orders at specified prices.
- **Market orders:** Execute against available resting liquidity.
- **Price-time priority:** Prioritize better prices and earlier orders at the same price level.
- **Partial fills:** Execute available quantities while retaining unfilled limit-order quantities.
- **Order cancellation:** Cancel resting orders by order ID.
- **Best bid and best ask:** Track the highest buy price and lowest sell price.
- **Bid-ask spread:** Calculate the difference between the best ask and best bid when both sides have orders.
- **Trade tracking:** Record execution prices and quantities.
- **Market analytics:** Display traded volume, trade count, traded notional, weighted average execution price, and order-book imbalance.
- **Interactive dashboard:** Explore the order book, market depth, and execution history using Streamlit and Plotly.
- **Automated testing:** Test matching behavior and bridge-level functionality.

## Tech Stack

| Technology | Purpose |
|---|---|
| C++17 | Order-book data structures and matching logic |
| STL (`map`, `list`, `vector`) | Order storage and price-level management |
| Python | Dashboard and integration testing |
| Streamlit | Interactive web interface |
| Pandas | Tabular data processing |
| Plotly | Market-depth and execution visualizations |
| JSON | Communication between the C++ bridge and Python dashboard |
| Git and GitHub | Version control and project hosting |

## Project Structure

```text
Project-2-Limit-Order-Book-Simulator/
├── main.cpp
├── bridge.cpp
├── test.cpp
├── test_bridge.py
├── app.py
├── requirements.txt
├── .gitignore
└── README.md
```

- `main.cpp` — Core C++ order-book implementation and matching functions.
- `bridge.cpp` — JSON-based interface connecting the C++ engine to the Python dashboard.
- `test.cpp` — C++ unit tests for order-book behavior.
- `test_bridge.py` — Python integration tests for the bridge.
- `app.py` — Streamlit dashboard and visualizations.
- `requirements.txt` — Python dependencies.
- `.gitignore` — Files and directories excluded from version control.

## How It Works

### 1. Order submission

A user submits a buy or sell order with an order ID, price, and quantity for limit orders. The engine evaluates whether the order can match existing liquidity on the opposite side.

### 2. Price-time priority

The book prioritizes the highest-priced buy orders and lowest-priced sell orders. When multiple orders share the same price, arrival order determines priority.

### 3. Trade execution

When the best buy price is greater than or equal to the best sell price, the orders can match. The execution quantity is the smaller of the two remaining quantities. If an order is only partially filled, its remaining quantity stays in the book when applicable.

### 4. Market data

The application derives market information from the current book and recorded trades, including the best bid, best ask, spread, execution history, and traded volume.

### 5. Visualization

The Streamlit dashboard communicates with the C++ bridge to display order-book levels, active orders, recent executions, and market analytics.

## Requirements

- A C++ compiler supporting C++17
- Python 3
- Git
- Visual Studio Code or another code editor

The following Python packages are required:

- Streamlit
- Pandas
- Plotly

## Installation and Setup

### 1. Clone the repository

```bash
git clone <YOUR_GITHUB_REPOSITORY_URL>
cd <YOUR_PROJECT_FOLDER>
```

Replace the placeholders with your actual repository URL and folder name.

### 2. Install Python dependencies

```bash
python -m pip install -r requirements.txt
```

### 3. Compile the C++ bridge

On Windows with MinGW:

```powershell
g++ -std=c++17 -O2 bridge.cpp -o bridge.exe
```

Ensure the executable name and path match the configuration in `app.py`.

### 4. Launch the dashboard

```powershell
python -m streamlit run app.py
```

Streamlit will display a local URL in the terminal. Open that URL in your browser.

## Running the Tests

### C++ unit tests

```powershell
g++ -std=c++17 -O2 test.cpp -o test_runner.exe
.\test_runner.exe
```

### Python integration tests

```powershell
python test_bridge.py
```

The integration tests may compile the bridge themselves. Ensure that a compatible C++ compiler is available on your system.

## Example Scenario

Suppose the order book receives the following orders:

| Order | Side | Price | Quantity |
|---|---|---:|---:|
| 1 | Sell | 100 | 10 |
| 2 | Buy | 100 | 4 |

The buy order matches four units of the resting sell order. After execution:

- Executed quantity: 4
- Remaining sell quantity: 6
- The trade is recorded in the execution history.
- The remaining sell order stays in the order book.

## Future Improvements

- Replace duplicated matching paths with a single authoritative matching engine.
- Add dedicated, tested best-bid, best-ask, and spread accessors.
- Add stronger input validation and edge-case coverage.
- Improve performance benchmarking under high order throughput.
- Add order-book replay and historical event recording.
- Add latency measurements and larger-scale simulation scenarios.
- Introduce more advanced exchange order types and risk controls.

## Learning Outcomes

This project explores:

- STL data structures and algorithmic complexity
- Price-time priority and matching-engine design
- State management and order lifecycle
- C++ and Python integration
- JSON-based process communication
- Automated unit and integration testing
- Interactive financial data visualization

## Disclaimer

This project is an educational simulator. It is not connected to a live exchange and is not intended for real-money trading.
