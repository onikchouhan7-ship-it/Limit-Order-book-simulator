"""Integration tests for the Streamlit dashboard's persistent C++ bridge.

Run from this directory with: python test_bridge.py
The script compiles bridge.cpp, sends commands to one persistent process, and
checks the JSON state returned after each command.
"""

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent


class BridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp_dir = tempfile.TemporaryDirectory(prefix="lob_bridge_tests_")
        exe_name = "bridge_test.exe" if __import__("os").name == "nt" else "bridge_test"
        cls.bridge_path = Path(cls.temp_dir.name) / exe_name
        compile_result = subprocess.run(
            ["g++", "-std=c++17", "-O2", str(ROOT / "bridge.cpp"), "-o", str(cls.bridge_path)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if compile_result.returncode != 0:
            raise RuntimeError("Could not compile bridge.cpp:\n" + compile_result.stderr)
        cls.process = subprocess.Popen(
            [str(cls.bridge_path)],
            cwd=ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    @classmethod
    def tearDownClass(cls):
        if getattr(cls, "process", None) and cls.process.poll() is None:
            try:
                cls.process.stdin.write("QUIT\n")
                cls.process.stdin.flush()
            except (BrokenPipeError, OSError):
                pass
            cls.process.terminate()
            cls.process.wait(timeout=5)
        cls.temp_dir.cleanup()

    def command(self, line):
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        output = self.process.stdout.readline()
        self.assertTrue(output, f"Bridge returned no output for command: {line}")
        return json.loads(output)

    def reset(self):
        result = self.command("RESET")
        self.assertTrue(result["ok"])
        return result

    def test_initial_and_reset_state_has_empty_book(self):
        state = self.reset()
        self.assertEqual(state["bids"], [])
        self.assertEqual(state["asks"], [])
        self.assertIsNone(state["best_bid"])
        self.assertIsNone(state["best_ask"])
        self.assertIsNone(state["spread"])
        self.assertEqual(state["trade_count"], 0)
        self.assertEqual(state["traded_volume"], 0)
        self.assertEqual(state["traded_notional"], 0)

    def test_best_bid_ask_spread_and_price_priority(self):
        self.reset()
        self.command("ADD 1 BUY 99 2")
        self.command("ADD 2 BUY 101 3")
        self.command("ADD 3 SELL 110 4")
        state = self.command("ADD 4 SELL 105 5")
        self.assertEqual(state["best_bid"], 101)
        self.assertEqual(state["best_ask"], 105)
        self.assertEqual(state["spread"], 4)
        self.assertEqual(state["bids"][0]["price"], 101)
        self.assertEqual(state["asks"][0]["price"], 105)

    def test_limit_order_partial_fill_and_resting_remainder(self):
        self.reset()
        self.command("ADD 10 SELL 100 10")
        state = self.command("ADD 11 BUY 100 4")
        self.assertEqual(state["trade_count"], 1)
        self.assertEqual(state["traded_volume"], 4)
        self.assertEqual(state["traded_notional"], 400)
        self.assertEqual(state["trades"][-1], {
            "buy_id": 11, "sell_id": 10, "price": 100, "quantity": 4})
        self.assertEqual(state["asks"][0]["quantity"], 6)
        self.assertEqual(state["active_orders"][0]["id"], 10)
        self.assertEqual(state["active_orders"][0]["quantity"], 6)

    def test_market_buy_consumes_best_asks_first(self):
        self.reset()
        self.command("ADD 20 SELL 102 5")
        self.command("ADD 21 SELL 101 3")
        state = self.command("MARKET 22 BUY 6")
        self.assertEqual(state["trade_count"], 2)
        self.assertEqual(state["traded_volume"], 6)
        self.assertEqual(state["traded_notional"], 609)
        self.assertEqual(state["trades"][0]["price"], 101)
        self.assertEqual(state["trades"][0]["quantity"], 3)
        self.assertEqual(state["trades"][1]["price"], 102)
        self.assertEqual(state["trades"][1]["quantity"], 3)
        self.assertEqual(state["asks"][0]["price"], 102)
        self.assertEqual(state["asks"][0]["quantity"], 2)
        self.assertTrue(all(order["id"] != 22 for order in state["active_orders"]))

    def test_market_order_unfilled_remainder_is_not_resting(self):
        self.reset()
        self.command("ADD 30 SELL 100 2")
        state = self.command("MARKET 31 BUY 5")
        self.assertEqual(state["traded_volume"], 2)
        self.assertEqual(state["asks"], [])
        self.assertTrue(all(order["id"] != 31 for order in state["active_orders"]))

    def test_cancel_buy_and_sell_orders(self):
        self.reset()
        self.command("ADD 40 BUY 99 2")
        self.command("ADD 41 SELL 105 3")
        cancelled_buy = self.command("CANCEL 40")
        self.assertTrue(cancelled_buy["ok"])
        self.assertEqual(cancelled_buy["bids"], [])
        cancelled_sell = self.command("CANCEL 41")
        self.assertTrue(cancelled_sell["ok"])
        self.assertEqual(cancelled_sell["asks"], [])
        self.assertFalse(self.command("CANCEL 999")["ok"])

    def test_reject_invalid_duplicate_and_unknown_commands(self):
        self.reset()
        self.assertFalse(self.command("ADD 1 BUY 0 5")["ok"])
        self.assertFalse(self.command("ADD 1 SIDEWAYS 100 5")["ok"])
        self.assertTrue(self.command("ADD 1 BUY 100 5")["ok"])
        self.assertFalse(self.command("ADD 1 SELL 101 5")["ok"])
        self.assertFalse(self.command("MARKET 2 SELL -1")["ok"])
        self.assertFalse(self.command("WHATEVER")["ok"])

    def test_trade_analytics_accumulate_across_multiple_fills(self):
        self.reset()
        self.command("ADD 60 SELL 100 2")
        state = self.command("ADD 61 BUY 100 2")
        self.command("ADD 62 SELL 110 3")
        state = self.command("MARKET 63 BUY 1")
        self.assertEqual(state["trade_count"], 2)
        self.assertEqual(state["traded_volume"], 3)
        self.assertEqual(state["traded_notional"], 310)
        self.assertEqual(state["trades"][-1]["price"], 110)


if __name__ == "__main__":
    unittest.main(verbosity=2)
