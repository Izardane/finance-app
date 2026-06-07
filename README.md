# Personal Finance Dashboard

Phase 1 is a native C++ app for manually entering a personal portfolio, earmarking parts of investments for goals, and generating a dashboard pie chart.

## Features

- Add or update investment categories such as bank FDs, bank balance, stocks, gold, or mutual funds.
- Add earmarks inside a category, for example marking part of a bank FD for vacation or a phone purchase.
- Prevent earmarks from exceeding the parent category amount.
- Show a home dashboard with total value, category percentages, and earmarks.
- Show a Qt Widgets GUI home page with a pie chart, total portfolio value, and category amounts.
- Show earmark allocations in the GUI as one horizontal bar per investment category.
- Export the current GUI database to `data/backups/`.
- Clear all GUI entries after automatically exporting a backup first.
- Import the latest exported backup later if needed.
- Generate `dashboard_pie.svg` from the current portfolio.
- Persist data locally in `data/portfolio.txt`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The GUI target requires Qt 6 Widgets, available on Ubuntu through:

```bash
sudo apt-get install -y qt6-base-dev
```

## Run

Terminal app:

```bash
./build/finance_dashboard
```

GUI app:

```bash
./build/finance_dashboard_gui
```

In the GUI, use `Category` to add investments. Use `Earmark` to pick an investment from the dropdown and allocate portions of it to purposes. The home page links to the earmark allocation screen and does not list earmarks directly.

The GUI uses Qt layouts, so the interface resizes with the window when maximized.

The GUI home page also has database actions:

- `Export`: writes a backup into `data/backups/`.
- `Clear all`: exports a backup first, then clears the active database.
- `Import latest`: restores the newest `.txt` backup from `data/backups/`.

The terminal app keeps you inside the category or earmark entry flow after saving an item. Leave the category name empty to return to the main menu.

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Design Notes

Money is stored as integer cents/paise instead of floating point values. That keeps financial calculations exact and avoids rounding drift.

The portfolio logic lives in `finance_core`, separate from the terminal UI. When you are ready for a graphical application, this core can be reused behind a desktop UI without changing the data model.
