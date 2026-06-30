#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import sys
from io import StringIO
from pathlib import Path
from typing import Any, Dict, Iterable, List

import requests


def plain_code(code: str) -> str:
    return str(code).strip().upper().replace("ETF.", "").split(".")[0]


def ts_code(code: str) -> str:
    c = plain_code(code)
    if c.startswith(("5", "6")):
        return f"{c}.SH"
    return f"{c}.SZ"


def market_symbol(code: str) -> str:
    c = plain_code(code)
    return ("sh" if c.startswith(("5", "6")) else "sz") + c


def load_universe(path: Path) -> List[str]:
    out: List[str] = []
    with path.open("r", encoding="utf-8-sig", newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            if str(row.get("enabled", "1")).strip() in {"0", "false", "False"}:
                continue
            code = plain_code(row.get("code", ""))
            if code:
                out.append(code)
    return out


def make_row(code: str, item: str) -> Dict[str, Any] | None:
    fields = item.split(",")
    if len(fields) < 7:
        return None
    try:
        return {
            "ts_code": ts_code(code),
            "trade_date": fields[0].replace("-", ""),
            "open": float(fields[1]),
            "close": float(fields[2]),
            "high": float(fields[3]),
            "low": float(fields[4]),
            "vol": float(fields[5]),
            "amount": float(fields[6]),
            "adj_factor": 1.0,
        }
    except Exception:
        return None


def fetch_em(code: str, start: str, end: str, adjust: str) -> List[Dict[str, Any]]:
    adjust_map = {"": "0", "qfq": "1", "hfq": "2"}
    c = plain_code(code)
    market_id = "1" if c.startswith(("5", "6")) else "0"
    session = requests.Session()
    session.trust_env = False
    response = session.get(
        "https://push2his.eastmoney.com/api/qt/stock/kline/get",
        params={
            "fields1": "f1,f2,f3,f4,f5,f6",
            "fields2": "f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61,f116",
            "ut": "7eea3edcaed734bea9cbfc24409ed989",
            "klt": "101",
            "fqt": adjust_map.get(adjust, "1"),
            "beg": start,
            "end": end,
            "secid": f"{market_id}.{c}",
        },
        headers={
            "User-Agent": "Mozilla/5.0 MyQuant/0.1",
            "Accept": "application/json,text/plain,*/*",
            "Referer": f"https://quote.eastmoney.com/{market_symbol(c)}.html",
        },
        timeout=20,
    )
    response.raise_for_status()
    payload = response.json()
    klines = ((payload.get("data") or {}).get("klines") or [])
    rows = [make_row(c, item) for item in klines]
    return [row for row in rows if row is not None]


def fetch_akshare(code: str, start: str, end: str, adjust: str, source: str) -> List[Dict[str, Any]]:
    import contextlib

    import akshare as ak

    c = plain_code(code)
    rows: List[Dict[str, Any]] = []
    with contextlib.redirect_stdout(StringIO()), contextlib.redirect_stderr(StringIO()):
        if source == "tx":
            df = ak.stock_zh_a_hist_tx(
                symbol=market_symbol(c),
                start_date=start,
                end_date=end,
                adjust=adjust,
                timeout=15,
            )
            for _, r in df.iterrows():
                rows.append({
                    "ts_code": ts_code(c),
                    "trade_date": str(r["date"]).replace("-", ""),
                    "open": float(r["open"]),
                    "high": float(r["high"]),
                    "low": float(r["low"]),
                    "close": float(r["close"]),
                    "vol": float(r.get("amount", 0)),
                    "amount": float(r.get("amount", 0)),
                    "adj_factor": 1.0,
                })
        elif source == "sina" and not adjust:
            df = ak.fund_etf_hist_sina(symbol=market_symbol(c))
            for _, r in df.iterrows():
                rows.append({
                    "ts_code": ts_code(c),
                    "trade_date": str(r["date"]).replace("-", ""),
                    "open": float(r["open"]),
                    "high": float(r["high"]),
                    "low": float(r["low"]),
                    "close": float(r["close"]),
                    "vol": float(r["volume"]),
                    "amount": float(r["volume"]) * float(r["close"]),
                    "adj_factor": 1.0,
                })
    return [row for row in rows if start <= row["trade_date"] <= end]


def fetch_one(code: str, start: str, end: str, adjust: str, sources: Iterable[str]) -> List[Dict[str, Any]]:
    last_error: Exception | None = None
    for source in sources:
        source = source.strip().lower()
        try:
            if source == "em":
                rows = fetch_em(code, start, end, adjust)
            elif source in {"tx", "sina"}:
                rows = fetch_akshare(code, start, end, adjust, source)
            else:
                continue
            if rows:
                return sorted(rows, key=lambda row: (row["trade_date"], row["ts_code"]))
        except Exception as exc:
            last_error = exc
    raise RuntimeError(f"{ts_code(code)} all sources failed: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--universe", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--start", required=True)
    parser.add_argument("--end", required=True)
    parser.add_argument("--adjust", default="qfq")
    parser.add_argument("--sources", default="em,tx,sina")
    args = parser.parse_args()

    codes = load_universe(Path(args.universe))
    sources = [s for s in args.sources.split(",") if s.strip()]
    all_rows: List[Dict[str, Any]] = []
    errors: Dict[str, str] = {}
    for code in codes:
        try:
            all_rows.extend(fetch_one(code, args.start, args.end, args.adjust, sources))
        except Exception as exc:
            errors[code] = str(exc)
    if not all_rows:
        print(json.dumps({"errors": errors}, ensure_ascii=False), file=sys.stderr)
        return 1

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(
            fh,
            fieldnames=["ts_code", "trade_date", "open", "high", "low", "close", "vol", "amount", "adj_factor"],
        )
        writer.writeheader()
        writer.writerows(sorted(all_rows, key=lambda row: (row["trade_date"], row["ts_code"])))
    if errors:
        print(json.dumps({"partial_errors": errors}, ensure_ascii=False), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
