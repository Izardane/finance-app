#include "finance/Portfolio.hpp"

#include <filesystem>
#include <iostream>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Check failed: " #condition << '\n'; \
            return 1; \
        } \
    } while (false)

int main() {
    finance::Portfolio portfolio;

    CHECK(finance::parse_money("1,234.50") == 123450);
    CHECK(finance::parse_money("100") == 10000);
    CHECK(finance::parse_money("100.999") == 10099);

    CHECK(portfolio.upsert_asset("Bank FD", 5000000));
    CHECK(portfolio.upsert_asset("Stocks", 2500000));
    CHECK(portfolio.total_cents() == 7500000);

    CHECK(portfolio.upsert_earmark("Bank FD", "Vacation", 1000000));
    CHECK(portfolio.upsert_earmark("Bank FD", "Phone", 500000));
    CHECK(portfolio.earmarked_total_cents("Bank FD") == 1500000);
    CHECK(!portfolio.validate());

    CHECK(!portfolio.upsert_earmark("Bank FD", "Too much", 6000000));
    CHECK(portfolio.earmarked_total_cents("Bank FD") == 1500000);
    CHECK(!portfolio.upsert_asset("Bank FD", 1000000));
    CHECK(portfolio.find_asset("Bank FD")->amount_cents == 5000000);

    finance::Portfolio large_allocation;
    CHECK(large_allocation.upsert_asset("Large FD", *finance::parse_money("369280")));
    CHECK(large_allocation.upsert_earmark("Large FD", "Goal", *finance::parse_money("341620")));
    CHECK(large_allocation.earmarked_total_cents("Large FD") == *finance::parse_money("341620"));

    const auto path = std::filesystem::temp_directory_path() / "finance_dashboard_portfolio_test.txt";
    CHECK(finance::save_portfolio(portfolio, path));
    const auto loaded = finance::load_portfolio(path);
    CHECK(loaded);
    CHECK(loaded->total_cents() == portfolio.total_cents());
    CHECK(loaded->find_asset("Bank FD") != nullptr);

    const auto svg_path = std::filesystem::temp_directory_path() / "finance_dashboard_test.svg";
    CHECK(finance::write_pie_chart_svg(portfolio, svg_path));
    CHECK(std::filesystem::exists(svg_path));

    return 0;
}
