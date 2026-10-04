# MyQuant Windows Migration Handoff

Updated: 2026-10-02

This document is a technical handoff for continuing MyQuant development with
Codex on Windows. It intentionally contains no brokerage account identifiers,
holding details, API keys, or other runtime data.

## Project State

- Repository: `https://github.com/qirunzeng/MyQuant.git`
- Branch: `main`
- Current source version: `0.2.0`
- Current HEAD: `35718ad Prevent duplicate MyQuant app registration`
- macOS bundle ID: `com.qirunzeng.myquant`
- Copyright: `Copyright 2026 qirunzeng. All rights reserved.`
- License: custom non-commercial source license in `LICENSE`
- Working tree was clean when this handoff was prepared.

MyQuant is a C++20, Qt 6, QML, CMake desktop application. The current code is
cross-platform in several places, but release engineering has only been
completed and tested on macOS. Do not assume the Windows package is ready just
because the source compiles conditionally.

## Product Direction and Completed Work

### ETF rotation

- Editable ETF universe, account cash, real positions, cost prices, and asset
  categories.
- Run configuration is persisted, including dates, capital, hold count, cash
  ratio, and stop loss.
- Local qfq market-data cache is checked first. AkShare is called only for
  missing warm-up/history or a missing end date.
- Backtest start-date warm-up is taken from dates before the requested start;
  the earlier incorrect forward-offset behavior was fixed.
- Backtest metrics include return, annualized return, drawdown, Calmar, cash,
  monthly results, current simulated holdings, and real-holding P/L.
- Charts support time-axis zoom, drag/pan, hover details, and BS markers.
- BS charts use one selected instrument at a time instead of a long grid.
- Operation suggestions are a separate view and must use saved, enabled ETF
  pool rows and the real-position snapshot. They are review material, not
  trade instructions.

### Review notes

- The old Markdown `notes` table is preserved.
- The structured review database uses month pages and chronological day/week/
  month timeline cards.
- Daily pages contain market context, watchlist observations, trades, and
  lessons. Weekly and monthly pages use different summary fields.
- Weeks appear after the days belonging to that week; labels use actual period
  keys rather than the literal words `Weekly` and `Monthly`.
- Records use forward-only SQLite migrations and soft deletion.
- Migration backups are created before schema upgrades.

### Dividend calendar and holdings

- Supports A-share, Hong Kong, and US holdings.
- Calendar shows record, ex-dividend, and payment dates. Weekends are omitted
  unless an actual event exists on that date.
- Deleted holdings do not contribute calendar events.
- Holdings and historical dividend estimates are combined. One-, three-, and
  five-year statistics are shown separately; one year is the default view.
- Buy/sell entries update quantity and cost basis. Transactions store estimated
  or manually corrected actual fees.
- Ex-dividend adjustments are idempotent and recorded separately.
- Current default fee settings are editable in Settings:
  - A-share stock commission: 0.741 per 10,000, minimum CNY 0.30.
  - A-share ETF commission: 0.5 per 10,000, minimum CNY 0.10.
  - Hong Kong and US defaults are configurable for the user's Webull account.
  - A-share sell stamp duty is separately configurable.
- Historical estimates are not promises of future dividends.

### UI and packaging

- Light and dark themes use a restrained VS Code/Typora-inspired layout.
- Main panes use resizable `SplitView` layouts where practical.
- Alternating rows and row buttons use inverse surface colors so controls keep
  visible boundaries.
- ETF, review notes, dividend calendar, and settings are the active modules.
  The old stock-selection research module was removed.
- macOS update/release scripts unregister and remove build copies to prevent
  duplicate MyQuant applications. This logic is macOS-specific.

## Architecture

Important source files:

- `src/AppPaths.*`: platform-specific runtime paths.
- `src/EtfRotationController.*`: ETF configuration, cache, backtest, portfolio,
  metrics, and suggestions.
- `src/NotesController.*`: legacy notes plus structured review database.
- `src/DividendController.*`: holdings, dividend events, forecasts, trades,
  fees, and cost adjustments.
- `src/SettingsController.*`: theme, Python path, fees, and local settings.
- `qml/pages/*.qml`: four main pages.
- `qml/components/AppTheme.qml`: shared colors and theme state.
- `tests/smoke_tests.cpp`: controller/database smoke coverage.

Runtime resources are copied next to the executable on non-Apple platforms.
Optional Python helpers are under `resources/scripts/`.

## Local Data and Privacy

Runtime data is deliberately excluded by `.gitignore`. Never add databases,
ETF configuration, exports, caches, brokerage screenshots, or account data to
Git or a GitHub Release.

macOS source location:

```text
~/Library/Application Support/MyQuant
```

Expected Windows destination from `AppPaths.cpp`:

```text
%LOCALAPPDATA%\MyQuant
```

Important files and directories:

```text
data/myquant.db              structured review records and legacy notes
data/dividends.db            dividend holdings, events, trades, adjustments
data/settings.json           theme, Python path, and fee settings
etf/config/etf_universe.csv  ETF pool
etf/config/real_position.csv real ETF positions
etf/config/account.json      ETF run/account configuration
etf/data/                    local qfq cache
backups/                     migration and manual safety copies
exports/                     local Markdown exports
```

Not every listed file is guaranteed to exist. Copy the entire runtime folder
over a private local channel, not through GitHub. Keep an untouched backup of
the macOS folder until Windows has opened and verified both databases.

Before copying, close MyQuant on both computers so SQLite and JSON/CSV files
are not being written. After copying, verify:

```powershell
Get-ChildItem "$env:LOCALAPPDATA\MyQuant" -Recurse
```

If Python is installed in a different Windows path, update it in Settings after
the first launch. Do not carry the macOS Python executable path forward.

## Brokerage Synchronization Status

A one-time local holdings snapshot was read from the already logged-in macOS
Tonghuashun client and merged into `data/dividends.db`. Before that merge, a
local database backup was created. No brokerage account number, shareholder ID,
password, or token was stored in MyQuant, and the ETF strategy universe was not
modified.

This is not a durable automatic integration. Tonghuashun for macOS did not
expose a reliable official holdings export or read-only API. Do not implement
background UI scraping and call it synchronization: it is brittle, hard to
audit, and could eventually click a trading control after a UI change.

Preferred future options, in order:

1. A broker-supported read-only API or documented QMT/terminal bridge.
2. A user-reviewed CSV/XLSX statement import with preview, validation, diff,
   transaction, backup, and rollback.
3. UI-assisted one-time capture only as an explicitly initiated fallback,
   never automatic trading and never credential storage.

Broker holdings must remain separate from the ETF strategy universe. Importing
all broker ETFs into `etf_universe.csv` would silently change strategy behavior.

## Windows Port: Known Gaps

Treat these as the first engineering tasks:

1. Add a `windows-release` configure/build preset to `CMakePresets.json`.
2. Install and target Qt 6.5+ with Core, Gui, Qml, Quick, QuickControls2, Sql,
   Network, and the SQLite driver. The project was developed against Qt 6.8.x.
3. Verify whether `MACOSX_BUNDLE` on `qt_add_executable` is harmless on Windows;
   make it conditional if CMake/Qt rejects it.
4. Fix the test environment: `TARGET_BUNDLE_DIR` in `set_tests_properties` is
   macOS-specific and may fail generation on Windows. Use the Windows Qt plugin
   directory or run through a deployed test environment.
5. Add a Windows `.ico` resource and executable metadata. The repository only
   has `.icns` packaging today.
6. Add a deployment script using `windeployqt`; stage to a clean directory and
   package only deployed binaries/resources, never runtime data.
7. Decide between a ZIP and installer. If adding an installer, preserve
   `%LOCALAPPDATA%\MyQuant` during upgrades and uninstalls unless the user
   explicitly requests data deletion.
8. Correct the runtime version in `src/main.cpp`, which still says `0.1.0`, to
   derive from or match CMake's `0.2.0`.
9. Run QML visual checks at narrow and wide Windows scaling levels, including
   100%, 125%, 150%, and 200%. Earlier bugs involved clipped right-hand panes,
   overlapping controls, and scrollbars covering content.
10. Test Python process invocation and path quoting on Windows, especially paths
    containing spaces or Chinese characters.

Suggested initial configure commands after adding the preset:

```powershell
git clone https://github.com/qirunzeng/MyQuant.git
cd MyQuant
cmake --preset windows-release -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build --preset windows-release
ctest --test-dir build/windows-release --output-on-failure
```

Use the actual installed Qt kit and compiler. Do not mix MinGW Qt libraries with
MSVC, or vice versa.

## Required Verification on Windows

- Application starts without missing QML module or SQLite driver errors.
- Light and dark themes render correctly.
- ETF configuration saves and reloads after restart.
- Changing only the backtest date recomputes results from local cache.
- Notes day/week/month cards persist and remain chronologically ordered.
- Negative values and percent-style text in summary fields do not clear while
  typing.
- Dividend holdings, trades, actual fees, calendar filtering, and automatic
  ex-dividend adjustment survive restart.
- Narrow windows scroll or reflow; controls do not overlap or disappear behind
  scrollbars.
- No runtime data appears in `git status` or release archives.
- Upgrade/reinstall preserves `%LOCALAPPDATA%\MyQuant`.

## Useful Git History

```text
35718ad Prevent duplicate MyQuant app registration
35f55ad Release MyQuant 0.2.0
4ad56a1 Add README screenshots
ea89c30 Persist ETF run configuration
8f31df1 Improve ETF holdings and chart interactions
458422b Initial MyQuant release
```

## Ready-to-Paste Prompt for Windows Codex

```text
Continue development of MyQuant from this repository and read
WINDOWS_HANDOFF.md first. Work ruthlessly: verify assumptions against the code,
do not overwrite runtime data, and never put personal databases, account data,
caches, exports, or credentials into Git. The immediate goal is to make the
existing Qt 6/CMake application build, test, deploy, and upgrade safely on
Windows while preserving %LOCALAPPDATA%\MyQuant. Add a Windows preset, fix any
platform-specific CMake/test issues, add Windows icon/version resources, create
a windeployqt packaging/update path, run smoke tests, then visually verify all
four pages at multiple DPI scales. Keep brokerage holdings separate from the
ETF strategy universe, and do not implement trade execution or brittle
background scraping of Tonghuashun.
```

