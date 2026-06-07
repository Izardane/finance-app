#include "finance/Portfolio.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace finance {
namespace {

constexpr Money kMaxReasonableAmount = 9'000'000'000'000'000LL;

std::string trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

std::string escape_field(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        if (ch == '\\' || ch == '|') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

std::vector<std::string> split_escaped(std::string_view line) {
    std::vector<std::string> fields;
    std::string current;
    bool escaped = false;
    for (const char ch : line) {
        if (escaped) {
            current.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '|') {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(current);
    return fields;
}

std::string svg_color(std::size_t index) {
    static constexpr const char* kColors[] = {
        "#2563eb", "#dc2626", "#16a34a", "#ca8a04", "#7c3aed",
        "#0891b2", "#db2777", "#4b5563", "#ea580c", "#0f766e"
    };
    return kColors[index % std::size(kColors)];
}

} // namespace

const std::vector<Asset>& Portfolio::assets() const noexcept {
    return assets_;
}

Money Portfolio::total_cents() const noexcept {
    return std::accumulate(assets_.begin(), assets_.end(), Money{0},
        [](Money total, const Asset& asset) { return total + asset.amount_cents; });
}

std::vector<PieSlice> Portfolio::distribution() const {
    const Money total = total_cents();
    std::vector<PieSlice> slices;
    slices.reserve(assets_.size());
    for (const Asset& asset : assets_) {
        slices.push_back(PieSlice{
            asset.name,
            asset.amount_cents,
            total > 0 ? (static_cast<double>(asset.amount_cents) * 100.0 / static_cast<double>(total)) : 0.0
        });
    }
    return slices;
}

std::optional<std::string> Portfolio::validate() const {
    for (const Asset& asset : assets_) {
        if (asset.name.empty()) {
            return "An investment category name cannot be empty.";
        }
        if (asset.amount_cents < 0) {
            return "Investment amounts cannot be negative.";
        }
        Money earmarked = 0;
        for (const Earmark& earmark : asset.earmarks) {
            if (earmark.name.empty()) {
                return "An earmark name cannot be empty.";
            }
            if (earmark.amount_cents < 0) {
                return "Earmark amounts cannot be negative.";
            }
            earmarked += earmark.amount_cents;
        }
        if (earmarked > asset.amount_cents) {
            return "Earmarks for " + asset.name + " exceed the category amount.";
        }
    }
    return std::nullopt;
}

bool Portfolio::upsert_asset(std::string name, Money amount_cents) {
    name = trim(name);
    if (name.empty() || amount_cents < 0 || amount_cents > kMaxReasonableAmount) {
        return false;
    }
    if (Asset* asset = find_asset_mut(name)) {
        if (earmarked_total_cents(name) > amount_cents) {
            return false;
        }
        asset->amount_cents = amount_cents;
        return true;
    }
    assets_.push_back(Asset{std::move(name), amount_cents, {}});
    std::sort(assets_.begin(), assets_.end(), [](const Asset& lhs, const Asset& rhs) {
        return lhs.name < rhs.name;
    });
    return true;
}

bool Portfolio::remove_asset(std::string_view name) {
    const auto original_size = assets_.size();
    std::erase_if(assets_, [name](const Asset& asset) { return asset.name == name; });
    return assets_.size() != original_size;
}

bool Portfolio::upsert_earmark(std::string_view asset_name, std::string earmark_name, Money amount_cents) {
    earmark_name = trim(earmark_name);
    if (earmark_name.empty() || amount_cents < 0 || amount_cents > kMaxReasonableAmount) {
        return false;
    }
    Asset* asset = find_asset_mut(asset_name);
    if (asset == nullptr) {
        return false;
    }

    auto it = std::find_if(asset->earmarks.begin(), asset->earmarks.end(),
        [&](const Earmark& earmark) { return earmark.name == earmark_name; });
    const Money current_amount = it == asset->earmarks.end() ? 0 : it->amount_cents;
    const Money projected_total = earmarked_total_cents(asset->name) - current_amount + amount_cents;
    if (projected_total > asset->amount_cents) {
        return false;
    }

    if (it == asset->earmarks.end()) {
        asset->earmarks.push_back(Earmark{std::move(earmark_name), amount_cents});
    } else {
        it->amount_cents = amount_cents;
    }
    return true;
}

Money Portfolio::earmarked_total_cents(std::string_view asset_name) const {
    const Asset* asset = find_asset(asset_name);
    if (asset == nullptr) {
        return 0;
    }
    return std::accumulate(asset->earmarks.begin(), asset->earmarks.end(), Money{0},
        [](Money total, const Earmark& earmark) { return total + earmark.amount_cents; });
}

const Asset* Portfolio::find_asset(std::string_view name) const noexcept {
    const auto it = std::find_if(assets_.begin(), assets_.end(),
        [name](const Asset& asset) { return asset.name == name; });
    return it == assets_.end() ? nullptr : &*it;
}

Asset* Portfolio::find_asset_mut(std::string_view name) noexcept {
    const auto it = std::find_if(assets_.begin(), assets_.end(),
        [name](const Asset& asset) { return asset.name == name; });
    return it == assets_.end() ? nullptr : &*it;
}

std::string format_money(Money cents) {
    const bool negative = cents < 0;
    if (negative) {
        cents = -cents;
    }
    std::ostringstream out;
    out << (negative ? "-" : "") << cents / 100 << '.'
        << std::setw(2) << std::setfill('0') << cents % 100;
    return out.str();
}

std::optional<Money> parse_money(std::string_view text) {
    const std::string value = trim(text);
    if (value.empty()) {
        return std::nullopt;
    }

    Money rupees = 0;
    Money paise = 0;
    bool seen_decimal = false;
    int decimal_digits = 0;

    for (const char ch : value) {
        if (ch == ',') {
            continue;
        }
        if (ch == '.') {
            if (seen_decimal) {
                return std::nullopt;
            }
            seen_decimal = true;
            continue;
        }
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        const int digit = ch - '0';
        if (!seen_decimal) {
            if (rupees > kMaxReasonableAmount / 10) {
                return std::nullopt;
            }
            rupees = rupees * 10 + digit;
        } else {
            if (decimal_digits < 2) {
                paise = paise * 10 + digit;
                ++decimal_digits;
            }
        }
    }
    if (decimal_digits == 1) {
        paise *= 10;
    }
    const Money cents = rupees * 100 + paise;
    if (cents > kMaxReasonableAmount) {
        return std::nullopt;
    }
    return cents;
}

bool save_portfolio(const Portfolio& portfolio, const std::filesystem::path& path) {
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    for (const Asset& asset : portfolio.assets()) {
        out << "ASSET|" << escape_field(asset.name) << '|' << asset.amount_cents << '\n';
        for (const Earmark& earmark : asset.earmarks) {
            out << "EARMARK|" << escape_field(asset.name) << '|'
                << escape_field(earmark.name) << '|' << earmark.amount_cents << '\n';
        }
    }
    return true;
}

std::optional<Portfolio> load_portfolio(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return Portfolio{};
    }

    Portfolio portfolio;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> fields = split_escaped(line);
        try {
            if (fields.size() == 3 && fields[0] == "ASSET") {
                if (!portfolio.upsert_asset(fields[1], std::stoll(fields[2]))) {
                    return std::nullopt;
                }
            } else if (fields.size() == 4 && fields[0] == "EARMARK") {
                if (!portfolio.upsert_earmark(fields[1], fields[2], std::stoll(fields[3]))) {
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        } catch (...) {
            return std::nullopt;
        }
    }
    if (portfolio.validate()) {
        return std::nullopt;
    }
    return portfolio;
}

bool write_pie_chart_svg(const Portfolio& portfolio, const std::filesystem::path& path) {
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream out(path);
    if (!out) {
        return false;
    }

    constexpr double cx = 180.0;
    constexpr double cy = 180.0;
    constexpr double radius = 130.0;
    constexpr double pi = 3.14159265358979323846;

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"720\" height=\"380\" viewBox=\"0 0 720 380\">\n";
    out << "<rect width=\"720\" height=\"380\" fill=\"#f8fafc\"/>\n";
    out << "<text x=\"32\" y=\"36\" font-family=\"Arial\" font-size=\"22\" font-weight=\"700\" fill=\"#111827\">Portfolio distribution</text>\n";

    const Money total = portfolio.total_cents();
    if (total <= 0) {
        out << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << radius << "\" fill=\"#e5e7eb\"/>\n";
        out << "<text x=\"110\" y=\"188\" font-family=\"Arial\" font-size=\"16\" fill=\"#374151\">No investments entered</text>\n";
    } else {
        double angle = -pi / 2.0;
        const auto slices = portfolio.distribution();
        for (std::size_t i = 0; i < slices.size(); ++i) {
            const double sweep = 2.0 * pi * static_cast<double>(slices[i].amount_cents) / static_cast<double>(total);
            const double end = angle + sweep;
            const double x1 = cx + radius * std::cos(angle);
            const double y1 = cy + radius * std::sin(angle);
            const double x2 = cx + radius * std::cos(end);
            const double y2 = cy + radius * std::sin(end);
            const int large_arc = sweep > pi ? 1 : 0;
            out << "<path d=\"M " << cx << ' ' << cy << " L " << x1 << ' ' << y1
                << " A " << radius << ' ' << radius << " 0 " << large_arc << " 1 "
                << x2 << ' ' << y2 << " Z\" fill=\"" << svg_color(i) << "\"/>\n";
            angle = end;
        }

        int legend_y = 86;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            out << "<rect x=\"400\" y=\"" << (legend_y - 13) << "\" width=\"14\" height=\"14\" fill=\"" << svg_color(i) << "\"/>\n";
            out << "<text x=\"424\" y=\"" << legend_y << "\" font-family=\"Arial\" font-size=\"14\" fill=\"#111827\">"
                << slices[i].label << " - " << std::fixed << std::setprecision(1) << slices[i].percentage
                << "% (" << format_money(slices[i].amount_cents) << ")</text>\n";
            legend_y += 28;
        }
    }

    out << "</svg>\n";
    return true;
}

} // namespace finance
