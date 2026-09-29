# MyQuant

MyQuant is a personal desktop research and review tool for ETF rotation, trading notes, dividend planning, and local settings.

This repository is an independent implementation. It does not copy source code, assets, brands, wording, or UI layouts from other products.

## Features

- ETF rotation review desk with AkShare/Python data refresh, local qfq cache, backtest metrics, trade details, and next-step review suggestions.
- Review database with month/week/day navigation, market context, watchlist, trade execution, weekly/monthly summaries, local SQLite storage, and Markdown export.
- Dividend calendar for A-share, Hong Kong, and US holdings, with editable events and historical 1/3/5-year cash-dividend estimates.
- Light and dark neutral themes, resizable VS Code-style work panes, and local settings for Python/AkShare, backup/export, and cache cleanup.

## Build

```bash
cmake --preset macos-release -DCMAKE_PREFIX_PATH="/path/to/Qt/6.8.3/macos"
cmake --build --preset macos-release
```

The macOS bundle is written to:

```text
build/macos-release/MyQuant.app
```

For a clean launchable copy:

```bash
tools/update_macos.sh
open ~/Applications/MyQuant.app
```

`tools/update_macos.sh` 是 macOS 的统一更新入口：它会构建、校验并替换
`~/Applications/MyQuant.app`，随后移除临时的构建目录 `.app` 并刷新
LaunchServices，避免系统同时显示两个 MyQuant。请不要直接启动
`build/macos-release/MyQuant.app`。

To create a GitHub-ready macOS archive without runtime data:

```bash
tools/release_macos.sh
```

The archive and its SHA-256 checksum are written to `dist/`.

## Data

Runtime data is stored under:

```text
~/Library/Application Support/MyQuant
```

Subdirectories include `data`, `logs`, `cache`, `etf`, and `exports`.

Runtime databases, settings, API keys, caches, exports, and account data are not
part of the source tree or release archive. Do not move files from this directory
into the repository when preparing a release.

Review records are stored in `data/myquant.db`. MyQuant keeps the legacy `notes` table and adds structured review tables for market days, watchlist items, trade executions, and period summaries. Schema upgrades are forward-only and create a database copy under `backups/` before migrating, so existing review data is not overwritten during app updates.

Dividend holdings and events are stored separately in `data/dividends.db`. Manual records are soft-deleted and are not overwritten by online refreshes. Forecasts are historical estimates rather than announced or guaranteed distributions; calendar dates are shown only when an event is available from the data source or entered manually.

## License

Copyright 2026 qirunzeng. All rights reserved.

This source package is published for personal, educational, and research use only. Commercial use is not permitted. See [LICENSE](LICENSE).

Third-party dependencies keep their own licenses and notices. Qt is used through the local Qt installation. Python, AkShare, pandas, and requests are optional runtime dependencies for live data fetching.

## Disclaimer

MyQuant provides research evidence and review material only. It does not provide financial advice or buy/sell instructions.
