#!/usr/bin/env python3
from __future__ import annotations

import argparse
import contextlib
import json
import re
from datetime import datetime
from io import StringIO
from pathlib import Path

import requests


def date_text(value) -> str:
    if value is None or str(value) in {"NaT", "nan", "None", ""}:
        return ""
    text = str(value).strip().split(" ")[0].replace("/", "-")
    for fmt in ("%Y-%m-%d", "%m-%d-%Y", "%m/%d/%Y"):
        try:
            return datetime.strptime(text, fmt).strftime("%Y-%m-%d")
        except ValueError:
            pass
    return text if re.fullmatch(r"\d{4}-\d{2}-\d{2}", text) else ""


def amount(value) -> float:
    match = re.search(r"[-+]?\d+(?:\.\d+)?", str(value).replace(",", ""))
    return float(match.group()) if match else 0.0


def fetch_a(holding):
    import akshare as ak
    code = str(holding["symbol"]).split(".")[0].zfill(6)
    with contextlib.redirect_stdout(StringIO()), contextlib.redirect_stderr(StringIO()):
        df = ak.stock_dividend_cninfo(symbol=code)
    rows = []
    for _, r in df.iterrows():
        rows.append({"market": "A", "symbol": code, "name": holding.get("name", ""),
            "declaration_date": date_text(r.get("实施方案公告日期")), "record_date": date_text(r.get("股权登记日")),
            "ex_date": date_text(r.get("除权日")), "pay_date": date_text(r.get("派息日")),
            "amount_per_share": amount(r.get("派息比例")) / 10.0, "currency": "CNY",
            "event_type": str(r.get("分红类型", "现金分红")), "source": "AkShare/巨潮资讯", "announced": 1,
            "notes": str(r.get("实施方案分红说明", ""))})
    return rows


def fetch_h(holding):
    import akshare as ak
    code = str(holding["symbol"]).split(".")[0].zfill(5)
    with contextlib.redirect_stdout(StringIO()), contextlib.redirect_stderr(StringIO()):
        df = ak.stock_hk_dividend_payout_em(symbol=code)
    rows = []
    for _, r in df.iterrows():
        plan = str(r.get("分红方案", ""))
        record = str(r.get("截至过户日", "")).split("-")[0]
        rows.append({"market": "H", "symbol": code, "name": holding.get("name", ""),
            "declaration_date": date_text(r.get("最新公告日期")), "record_date": date_text(record),
            "ex_date": date_text(r.get("除净日")), "pay_date": date_text(r.get("发放日")),
            "amount_per_share": amount(plan), "currency": "HKD", "event_type": str(r.get("分配类型", "现金分红")),
            "source": "AkShare/东方财富", "announced": 1, "notes": plan})
    return rows


def fetch_us(holding):
    symbol = str(holding["symbol"]).upper()
    session = requests.Session(); session.trust_env = False
    response = session.get(f"https://api.nasdaq.com/api/quote/{symbol}/dividends",
        params={"assetclass": "stocks"}, headers={"User-Agent": "Mozilla/5.0 MyQuant/0.1", "Accept": "application/json"}, timeout=20)
    response.raise_for_status()
    rows = (((response.json().get("data") or {}).get("dividends") or {}).get("rows") or [])
    return [{"market": "US", "symbol": symbol, "name": holding.get("name", ""),
        "declaration_date": date_text(r.get("declarationDate")), "record_date": date_text(r.get("recordDate")),
        "ex_date": date_text(r.get("exOrEffDate")), "pay_date": date_text(r.get("paymentDate")),
        "amount_per_share": amount(r.get("amount")), "currency": r.get("currency") or "USD",
        "event_type": r.get("type") or "现金分红", "source": "Nasdaq", "announced": 1, "notes": ""} for r in rows]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--holdings", required=True); parser.add_argument("--output", required=True)
    args = parser.parse_args()
    holdings = json.loads(Path(args.holdings).read_text(encoding="utf-8"))
    events, errors = [], {}
    for holding in holdings:
        key = f"{holding.get('market')}:{holding.get('symbol')}"
        try:
            events.extend({"A": fetch_a, "H": fetch_h, "US": fetch_us}[holding["market"]](holding))
        except Exception as exc:
            errors[key] = str(exc)
    Path(args.output).write_text(json.dumps({"events": events, "errors": errors}, ensure_ascii=False), encoding="utf-8")
    return 0 if events or not holdings else 1


if __name__ == "__main__":
    raise SystemExit(main())
