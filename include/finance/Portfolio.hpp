#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace finance {

using Money = std::int64_t;

struct Earmark {
    std::string name;
    Money amount_cents{};
};

struct Asset {
    std::string name;
    Money amount_cents{};
    std::vector<Earmark> earmarks;
};

struct PieSlice {
    std::string label;
    Money amount_cents{};
    double percentage{};
};

class Portfolio {
public:
    [[nodiscard]] const std::vector<Asset>& assets() const noexcept;
    [[nodiscard]] Money total_cents() const noexcept;
    [[nodiscard]] std::vector<PieSlice> distribution() const;
    [[nodiscard]] std::optional<std::string> validate() const;

    bool upsert_asset(std::string name, Money amount_cents);
    bool remove_asset(std::string_view name);
    bool upsert_earmark(std::string_view asset_name, std::string earmark_name, Money amount_cents);

    [[nodiscard]] Money earmarked_total_cents(std::string_view asset_name) const;
    [[nodiscard]] const Asset* find_asset(std::string_view name) const noexcept;

private:
    std::vector<Asset> assets_;

    Asset* find_asset_mut(std::string_view name) noexcept;
};

[[nodiscard]] std::string format_money(Money cents);
[[nodiscard]] std::optional<Money> parse_money(std::string_view text);

bool save_portfolio(const Portfolio& portfolio, const std::filesystem::path& path);
[[nodiscard]] std::optional<Portfolio> load_portfolio(const std::filesystem::path& path);
bool write_pie_chart_svg(const Portfolio& portfolio, const std::filesystem::path& path);

} // namespace finance
