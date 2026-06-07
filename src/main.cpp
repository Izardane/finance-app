#include "finance/Portfolio.hpp"

#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

std::string data_dir() {
    const char* env = std::getenv("FINANCE_DATA_DIR");
    return env ? env : "data";
}

std::string kDataFile() { return data_dir() + "/portfolio.txt"; }
std::string kChartFile() { return data_dir() + "/dashboard_pie.svg"; }

std::string mask_str(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == ',') {
            out += "\u2022";
        } else {
            out += c;
        }
    }
    return out;
}

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

void print_dashboard(const finance::Portfolio& portfolio, bool masked = false) {
    std::cout << "\nHome dashboard\n";
    const auto tf = [masked](finance::Money c) { return masked ? mask_str(finance::format_money(c)) : finance::format_money(c); };
    std::cout << "Total portfolio: " << tf(portfolio.total_cents()) << "\n\n";

    const auto slices = portfolio.distribution();
    if (slices.empty()) {
        std::cout << "No portfolio entries yet.\n";
        return;
    }

    for (const auto& slice : slices) {
        const int bar = static_cast<int>(slice.percentage / 2.0);
        std::cout << std::left << std::setw(22) << slice.label
                  << std::right << std::setw(14) << tf(slice.amount_cents)
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
                      << ": " << tf(earmark.amount_cents) << '\n';
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

void add_or_update_earmarks(finance::Portfolio& portfolio, bool masked = false) {
    while (true) {
        if (portfolio.assets().empty()) {
            std::cout << "Add an investment category first.\n";
            return;
        }

        std::cout << "\nAdd/update earmark\n";
        std::cout << "Existing categories:\n";
        const auto tf = [masked](finance::Money c) { return masked ? mask_str(finance::format_money(c)) : finance::format_money(c); };
        for (const auto& asset : portfolio.assets()) {
            const auto available = asset.amount_cents - portfolio.earmarked_total_cents(asset.name);
            std::cout << "  " << asset.name << " (" << tf(available) << " available)\n";
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
    if (const auto loaded = finance::load_portfolio(kDataFile())) {
        portfolio = *loaded;
    } else {
        std::cout << "Could not read existing portfolio data. Starting with an empty portfolio.\n";
    }

    bool privacy_hidden = true;
    while (true) {
        std::cout << "\nPersonal Finance Dashboard\n";
        std::cout << "1. Home dashboard\n";
        std::cout << "2. Add/update investment category\n";
        std::cout << "3. Add/update earmark\n";
        std::cout << "4. Save and generate pie chart\n";
        std::cout << "5. Quit\n";
        std::cout << "6. Toggle privacy (currently: " << (privacy_hidden ? "HIDDEN" : "VISIBLE") << ")\n";
        std::cout << "Choice: ";

        int choice = 0;
        std::cin >> choice;
        if (!std::cin) {
            std::cin.clear();
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
        case 1:
            print_dashboard(portfolio, privacy_hidden);
            break;
        case 2:
            add_or_update_assets(portfolio);
            break;
        case 3:
            add_or_update_earmarks(portfolio, privacy_hidden);
            break;
        case 4:
            if (const auto error = portfolio.validate()) {
                std::cout << "Cannot save: " << *error << '\n';
            } else if (finance::save_portfolio(portfolio, kDataFile())
                    && finance::write_pie_chart_svg(portfolio, kChartFile())) {
                std::cout << "Saved to " << std::filesystem::absolute(kDataFile()) << '\n';
                std::cout << "Pie chart generated at " << std::filesystem::absolute(kChartFile()) << '\n';
            } else {
                std::cout << "Save failed.\n";
            }
            break;
        case 5:
            if (const auto error = portfolio.validate()) {
                std::cout << "Not saving invalid portfolio: " << *error << '\n';
                return 1;
            }
            finance::save_portfolio(portfolio, kDataFile());
            finance::write_pie_chart_svg(portfolio, kChartFile());
            return 0;
        case 6:
            privacy_hidden = !privacy_hidden;
            break;
        default:
            std::cout << "Choose one of the listed options.\n";
            break;
        }
    }
}
