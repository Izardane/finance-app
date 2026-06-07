#include "finance/Portfolio.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

constexpr auto kDataFile = "data/portfolio.txt";
constexpr auto kChartFile = "dashboard_pie.svg";

std::string prompt_line(const std::string& label) {
    std::cout << label;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

finance::Money prompt_money(const std::string& label) {
    while (true) {
        const std::string input = prompt_line(label);
        if (const auto amount = finance::parse_money(input)) {
            return *amount;
        }
        std::cout << "Enter a valid amount, for example 25000 or 25000.50.\n";
    }
}

void print_dashboard(const finance::Portfolio& portfolio) {
    std::cout << "\nHome dashboard\n";
    std::cout << "Total portfolio: " << finance::format_money(portfolio.total_cents()) << "\n\n";

    const auto slices = portfolio.distribution();
    if (slices.empty()) {
        std::cout << "No portfolio entries yet.\n";
        return;
    }

    for (const auto& slice : slices) {
        const int bar = static_cast<int>(slice.percentage / 2.0);
        std::cout << std::left << std::setw(22) << slice.label
                  << std::right << std::setw(14) << finance::format_money(slice.amount_cents)
                  << "  " << std::setw(5) << std::fixed << std::setprecision(1) << slice.percentage << "%  ";
        for (int i = 0; i < bar; ++i) {
            std::cout << '#';
        }
        std::cout << '\n';
    }

    std::cout << "\nEarmarks\n";
    bool has_earmarks = false;
    for (const auto& asset : portfolio.assets()) {
        for (const auto& earmark : asset.earmarks) {
            has_earmarks = true;
            std::cout << "  " << asset.name << " -> " << earmark.name
                      << ": " << finance::format_money(earmark.amount_cents) << '\n';
        }
    }
    if (!has_earmarks) {
        std::cout << "  None yet.\n";
    }
}

void add_or_update_assets(finance::Portfolio& portfolio) {
    while (true) {
        std::cout << "\nAdd/update investment category\n";
        std::cout << "Leave the category name empty to return to the main menu.\n";
        const std::string name = prompt_line("Investment category, e.g. Bank FD, Bank Balance, Stocks: ");
        if (name.empty()) {
            return;
        }
        const finance::Money amount = prompt_money("Amount: ");
        if (!portfolio.upsert_asset(name, amount)) {
            std::cout << "Could not save this category. Check the name and amount.\n";
            continue;
        }
        if (const auto error = portfolio.validate()) {
            std::cout << *error << '\n';
        } else {
            std::cout << "Category saved. Add another category or leave the name empty to exit.\n";
        }
    }
}

void add_or_update_earmarks(finance::Portfolio& portfolio) {
    while (true) {
        if (portfolio.assets().empty()) {
            std::cout << "Add an investment category first.\n";
            return;
        }

        std::cout << "\nAdd/update earmark\n";
        std::cout << "Existing categories:\n";
        for (const auto& asset : portfolio.assets()) {
            const auto available = asset.amount_cents - portfolio.earmarked_total_cents(asset.name);
            std::cout << "  " << asset.name << " (" << finance::format_money(available) << " available)\n";
        }

        std::cout << "Leave the category name empty to return to the main menu.\n";
        const std::string asset_name = prompt_line("Category to earmark from: ");
        if (asset_name.empty()) {
            return;
        }
        const std::string earmark_name = prompt_line("Earmark purpose, e.g. Vacation, Phone: ");
        const finance::Money amount = prompt_money("Earmark amount: ");
        if (!portfolio.upsert_earmark(asset_name, earmark_name, amount)) {
            std::cout << "Could not save this earmark. It may exceed the category amount.\n";
            continue;
        }
        std::cout << "Earmark saved. Add another earmark or leave the category name empty to exit.\n";
    }
}

} // namespace

int main() {
    finance::Portfolio portfolio;
    if (const auto loaded = finance::load_portfolio(kDataFile)) {
        portfolio = *loaded;
    } else {
        std::cout << "Could not read existing portfolio data. Starting with an empty portfolio.\n";
    }

    while (true) {
        std::cout << "\nPersonal Finance Dashboard\n";
        std::cout << "1. Home dashboard\n";
        std::cout << "2. Add/update investment category\n";
        std::cout << "3. Add/update earmark\n";
        std::cout << "4. Save and generate pie chart\n";
        std::cout << "5. Quit\n";
        std::cout << "Choice: ";

        int choice = 0;
        std::cin >> choice;
        if (!std::cin) {
            std::cin.clear();
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
        case 1:
            print_dashboard(portfolio);
            break;
        case 2:
            add_or_update_assets(portfolio);
            break;
        case 3:
            add_or_update_earmarks(portfolio);
            break;
        case 4:
            if (const auto error = portfolio.validate()) {
                std::cout << "Cannot save: " << *error << '\n';
            } else if (finance::save_portfolio(portfolio, kDataFile)
                    && finance::write_pie_chart_svg(portfolio, kChartFile)) {
                std::cout << "Saved to " << std::filesystem::absolute(kDataFile) << '\n';
                std::cout << "Pie chart generated at " << std::filesystem::absolute(kChartFile) << '\n';
            } else {
                std::cout << "Save failed.\n";
            }
            break;
        case 5:
            if (const auto error = portfolio.validate()) {
                std::cout << "Not saving invalid portfolio: " << *error << '\n';
                return 1;
            }
            finance::save_portfolio(portfolio, kDataFile);
            finance::write_pie_chart_svg(portfolio, kChartFile);
            return 0;
        default:
            std::cout << "Choose one of the listed options.\n";
            break;
        }
    }
}
