#include "finance/Portfolio.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <QButtonGroup>
#include <QCheckBox>
#include <QInputDialog>
#include <QMenu>
#include <QRadioButton>
#include <QScrollArea>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace {

constexpr auto kDataFile = "data/portfolio.txt";
constexpr auto kChartFile = "dashboard_pie.svg";
constexpr auto kBackupDir = "data/backups";

QString qstr(const std::string& value) {
    return QString::fromStdString(value);
}

std::string str(const QString& value) {
    return value.trimmed().toStdString();
}

QString fmt(const finance::Money& cents, bool masked = false) {
    if (masked) {
        auto s = finance::format_money(cents);
        std::string out;
        for (char c : s) {
            if (std::isdigit(static_cast<unsigned char>(c)) || c == ',') {
                out += "\u2022";
            } else {
                out += c;
            }
        }
        return qstr(out);
    }
    return qstr(finance::format_money(cents));
}

QColor palette_color(std::size_t index) {
    static const std::array<QColor, 10> colors{
        QColor("#2563eb"), QColor("#dc2626"), QColor("#16a34a"), QColor("#ca8a04"),
        QColor("#7c3aed"), QColor("#0891b2"), QColor("#db2777"), QColor("#4b5563"),
        QColor("#ea580c"), QColor("#0f766e")
    };
    return colors[index % colors.size()];
}

class PieChart final : public QWidget {
public:
    explicit PieChart(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(280, 320);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
    }

    void set_portfolio(const finance::Portfolio* portfolio, bool privacy = false) {
        portfolio_ = portfolio;
        privacy_ = privacy;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        constexpr int legend_bottom_margin = 90;
        const QRectF bounds = rect().adjusted(18, 18, -18, -18 - legend_bottom_margin);
        const qreal side = std::min(bounds.width(), bounds.height());
        const QRectF pie(bounds.center().x() - side / 2.0,
                         bounds.top() + std::max(0.0, (bounds.height() - side) / 2.0),
                         side, side);
        last_pie_rect_ = pie;

        painter.setPen(Qt::NoPen);
        if (portfolio_ == nullptr || portfolio_->total_cents() <= 0) {
            painter.setBrush(QColor("#e2e8f0"));
            painter.drawEllipse(pie);
            painter.setPen(QColor("#64748b"));
            painter.drawText(pie, Qt::AlignCenter, "No data yet");
            return;
        }

        const auto slices = portfolio_->distribution();
        const finance::Money total = portfolio_->total_cents();

        slice_ranges_.clear();
        int start_angle = 90 * 16;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            const int span = static_cast<int>(std::round(360.0 * 16.0 * slices[i].amount_cents / total));
            const int end_angle = start_angle - span;
            painter.setBrush(palette_color(i));
            painter.drawPie(pie, start_angle, -span);
            slice_ranges_.push_back({end_angle, start_angle});
            start_angle = end_angle;
        }

        const QRectF inner = pie.adjusted(side * 0.32, side * 0.32, -side * 0.32, -side * 0.32);
        painter.setBrush(QColor("#ffffff"));
        painter.drawEllipse(inner);

        draw_legend(painter, slices, pie);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (portfolio_ == nullptr || portfolio_->total_cents() <= 0 || slice_ranges_.empty()) {
            setToolTip("");
            return;
        }

        const QRectF& pie = last_pie_rect_;
        const QPointF center = pie.center();
        const qreal radius = pie.width() / 2.0;
        const qreal inner_r = radius * 0.32;

        const double dx = event->position().x() - center.x();
        const double dy = event->position().y() - center.y();
        const double dist = std::sqrt(dx * dx + dy * dy);

        if (dist > radius || dist < inner_r) {
            setToolTip("");
            return;
        }

        constexpr double kPi = 3.14159265358979323846;
        double angle_deg = std::atan2(-dy, dx) * 180.0 / kPi;
        if (angle_deg < 0) angle_deg += 360.0;
        const int qt_angle = static_cast<int>(std::round(angle_deg * 16.0));

        auto normalize = [](int a) {
            int r = a % (360 * 16);
            if (r < 0) r += 360 * 16;
            return r;
        };

        const auto slices = portfolio_->distribution();
        for (std::size_t i = 0; i < slices.size(); ++i) {
            const int s = normalize(slice_ranges_[i].second);
            const int e = normalize(slice_ranges_[i].first);

            bool in_slice = false;
            if (s >= e) {
                in_slice = qt_angle > e && qt_angle <= s;
            } else {
                in_slice = qt_angle > e || qt_angle <= s;
            }

            if (in_slice) {
                const auto& slice = slices[i];
                setToolTip(QString("%1\n%2  (%3%)")
                    .arg(qstr(slice.label))
                    .arg(fmt(slice.amount_cents, privacy_))
                    .arg(slice.percentage, 0, 'f', 1));
                return;
            }
        }
        setToolTip("");
    }

private:
    void draw_legend(QPainter& painter, const std::vector<finance::PieSlice>& slices, const QRectF& pie) {
        int y = static_cast<int>(pie.bottom() + 16);
        int x = 24;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            painter.setBrush(palette_color(i));
            painter.setPen(Qt::NoPen);
            painter.drawRect(x, y, 12, 12);
            painter.setPen(QColor("#0f172a"));
            const QColor text_color = privacy_ ? QColor("#94a3b8") : QColor("#0f172a");
            painter.setPen(text_color);
            const QString text = fmt(slices[i].amount_cents, privacy_);
            painter.drawText(x + 18, y + 11, qstr(slices[i].label + "  ") + text);

            x += 18 + painter.fontMetrics().horizontalAdvance(text) + 16;
            if (x > width() - 60) {
                x = 24;
                y += 24;
            }
        }
    }

    const finance::Portfolio* portfolio_{};
    bool privacy_{};
    QRectF last_pie_rect_;
    std::vector<std::pair<int, int>> slice_ranges_;
};

class AllocationChart final : public QWidget {
public:
    explicit AllocationChart(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void set_portfolio(finance::Portfolio* portfolio, bool privacy = false) {
        portfolio_ = portfolio;
        privacy_ = privacy;
        update();
        updateGeometry();
    }

    std::function<void()> on_edit;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#ffffff"));

        hit_regions_.clear();

        if (portfolio_ == nullptr || portfolio_->assets().empty()) {
            painter.setPen(QColor("#64748b"));
            painter.drawText(rect(), Qt::AlignCenter, "Add investment categories to start allocating earmarks.");
            return;
        }

        QFont label_font = painter.font();
        label_font.setBold(true);
        int y = 26;
        for (const auto& asset : portfolio_->assets()) {
            painter.setFont(label_font);
            painter.setPen(QColor("#0f172a"));
            painter.drawText(24, y, qstr(asset.name + "  ") + fmt(asset.amount_cents, privacy_));
            y += 14;

            const QRect bar(24, y, width() - 48, 28);
            painter.setPen(QColor("#cbd5e1"));
            painter.setBrush(QColor("#f1f5f9"));
            painter.drawRoundedRect(bar, 6, 6);

            int x = bar.x();
            for (std::size_t i = 0; i < asset.earmarks.size(); ++i) {
                const int segment_width = asset.amount_cents > 0
                    ? std::max(1, static_cast<int>(std::round(static_cast<double>(bar.width()) * asset.earmarks[i].amount_cents / asset.amount_cents)))
                    : 0;
                const int clamped = std::min(segment_width, bar.right() + 1 - x);
                if (clamped > 0) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(palette_color(i));
                    const QRect seg_rect(x, bar.y(), clamped, bar.height());
                    painter.drawRoundedRect(seg_rect, 5, 5);
                    hit_regions_.push_back({asset.name, asset.earmarks[i].name, seg_rect});
                    x += clamped;
                }
            }

            painter.setFont(QFont{});
            painter.setPen(QColor("#475569"));
            y += 48;
            int legend_x = 24;
            for (std::size_t i = 0; i < asset.earmarks.size(); ++i) {
                painter.setBrush(palette_color(i));
                painter.setPen(Qt::NoPen);
                painter.drawRect(legend_x, y - 10, 10, 10);
                painter.setPen(QColor("#475569"));
                const QString label = qstr(asset.earmarks[i].name + " ") + fmt(asset.earmarks[i].amount_cents, privacy_);
                painter.drawText(legend_x + 16, y, label);
                legend_x += 180;
                if (legend_x > width() - 160) {
                    legend_x = 24;
                    y += 22;
                }
            }
            if (asset.earmarks.empty()) {
                painter.drawText(24, y, "No earmarks allocated.");
            }
            y += 38;
        }
    }

    QSize minimumSizeHint() const override {
        if (!portfolio_ || portfolio_->assets().empty()) {
            return QSize(400, 260);
        }
        int h = 30;
        for (const auto& asset : portfolio_->assets()) {
            h += 14 + 4 + 28 + 4;
            int legend_items = static_cast<int>(asset.earmarks.size());
            if (legend_items > 0) {
                int cols = std::max(1, (width() - 80) / 180);
                int rows = (legend_items + cols - 1) / cols;
                h += rows * 22;
            } else {
                h += 22;
            }
            h += 38;
        }
        return QSize(400, h + 10);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::RightButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        for (const auto& region : hit_regions_) {
            if (region.rect.contains(event->position().toPoint())) {
                show_edit_menu(region.asset_name, region.earmark_name,
                               event->globalPosition().toPoint());
                return;
            }
        }
    }

private:
    struct HitRegion {
        std::string asset_name;
        std::string earmark_name;
        QRect rect;
    };

    void show_edit_menu(const std::string& asset_name, const std::string& earmark_name,
                        QPoint global_pos) {
        QMenu menu;
        QAction* action = menu.addAction("Edit earmark");
        QAction* chosen = menu.exec(global_pos);
        if (chosen != action) return;

        const auto* asset = portfolio_->find_asset(asset_name);
        if (!asset) return;
        auto it = std::find_if(asset->earmarks.begin(), asset->earmarks.end(),
            [&](const auto& e) { return e.name == earmark_name; });
        if (it == asset->earmarks.end()) return;

        bool ok = false;
        const double current = static_cast<double>(it->amount_cents) / 100.0;
        const double new_val = QInputDialog::getDouble(
            this, "Edit earmark",
            QString("Edit amount earmarked for '%1':").arg(qstr(earmark_name)),
            current, 0, 999999999999.99, 4, &ok);
        if (!ok) return;

        const finance::Money new_cents = static_cast<finance::Money>(std::round(new_val * 100.0));
        if (!portfolio_->upsert_earmark(asset_name, earmark_name, new_cents)) {
            QMessageBox::warning(this, "Could not update",
                "This allocation would exceed the available category amount.");
            return;
        }
        if (on_edit) on_edit();
        update();
    }

    finance::Portfolio* portfolio_{};
    bool privacy_{};
    std::vector<HitRegion> hit_regions_;
};

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("Personal Finance Dashboard");
        resize(1100, 720);
        setMinimumSize(860, 560);

        if (const auto loaded = finance::load_portfolio(kDataFile)) {
            portfolio_ = *loaded;
        }

        tabs_ = new QTabWidget;
        setCentralWidget(tabs_);

        build_home_tab();
        build_category_tab();
        build_earmark_tab();
        apply_style();
        refresh_all();
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        save();
        QMainWindow::closeEvent(event);
    }

private:
    finance::Portfolio portfolio_;
    QTabWidget* tabs_{};
    QLabel* total_label_{};
    QPushButton* privacy_btn_{};
    bool privacy_hidden_{true};
    PieChart* pie_chart_{};
    QTableWidget* category_table_{};
    QLineEdit* category_name_{};
    QLineEdit* category_amount_{};
    QRadioButton* category_replace_{};
    QRadioButton* category_add_{};
    QComboBox* earmark_category_{};
    QLabel* available_label_{};
    QLineEdit* earmark_purpose_{};
    QLineEdit* earmark_amount_{};
    AllocationChart* allocation_chart_{};

    void build_home_tab() {
        auto* page = new QWidget;
        auto* root = new QVBoxLayout(page);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(18);

        auto* top = new QHBoxLayout;
        auto* summary = card();
        auto* summary_layout = new QVBoxLayout(summary);
        auto* total_title_widget = new QWidget;
        auto* total_title_layout = new QHBoxLayout(total_title_widget);
        total_title_layout->setContentsMargins(0, 0, 0, 0);
        auto* total_title = new QLabel("Total portfolio");
        total_title->setObjectName("muted");
        privacy_btn_ = new QPushButton(QString::fromUtf8("\xF0\x9F\x91\x81\xE2\x80\x8D\xE2\x97\xA8\xEF\xB8\x8F"));
        privacy_btn_->setFixedSize(28, 28);
        privacy_btn_->setToolTip("Toggle privacy (hide/show amounts)");
        privacy_btn_->setCursor(Qt::PointingHandCursor);
        privacy_btn_->setStyleSheet("QPushButton { background: transparent; border: 1px solid #cbd5e1; border-radius: 14px; font-size: 14px; padding: 0; } QPushButton:hover { background: #e2e8f0; }");
        connect(privacy_btn_, &QPushButton::clicked, this, [this] { toggle_privacy(); });
        total_title_layout->addWidget(total_title);
        total_title_layout->addStretch(1);
        total_title_layout->addWidget(privacy_btn_);
        total_label_ = new QLabel;
        total_label_->setObjectName("total");
        pie_chart_ = new PieChart;
        summary_layout->addWidget(total_title_widget);
        summary_layout->addWidget(total_label_);
        summary_layout->addWidget(pie_chart_, 1);

        auto* categories = card();
        auto* categories_layout = new QVBoxLayout(categories);
        auto* title = new QLabel("Categories");
        title->setObjectName("sectionTitle");
        category_table_ = new QTableWidget(0, 3);
        category_table_->setHorizontalHeaderLabels({"Category", "Amount", "%"});
        category_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        category_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        category_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        category_table_->verticalHeader()->hide();
        category_table_->setSelectionMode(QAbstractItemView::NoSelection);
        category_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        categories_layout->addWidget(title);
        categories_layout->addWidget(category_table_, 1);

        top->addWidget(summary, 1);
        top->addWidget(categories, 1);
        root->addLayout(top, 1);

        auto* actions = new QHBoxLayout;
        auto* view_earmarks = new QPushButton("View earmarks");
        auto* export_button = new QPushButton("Export");
        auto* import_button = new QPushButton("Import latest");
        auto* clear_button = new QPushButton("Clear all");
        clear_button->setObjectName("danger");
        actions->addWidget(view_earmarks);
        actions->addStretch(1);
        actions->addWidget(export_button);
        actions->addWidget(import_button);
        actions->addWidget(clear_button);
        root->addLayout(actions);

        connect(category_table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            if (const auto* item = category_table_->item(row, 0)) {
                edit_category(str(item->text()));
            }
        });
        connect(view_earmarks, &QPushButton::clicked, this, [this] { tabs_->setCurrentIndex(2); });
        connect(export_button, &QPushButton::clicked, this, [this] { export_backup(true); });
        connect(import_button, &QPushButton::clicked, this, [this] { import_latest_backup(); });
        connect(clear_button, &QPushButton::clicked, this, [this] { clear_all_entries(); });

        tabs_->addTab(page, "Home");
    }

    void build_category_tab() {
        auto* page = new QWidget;
        auto* root = new QHBoxLayout(page);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(18);

        auto* form_card = card();
        auto* form_layout = new QVBoxLayout(form_card);
        auto* title = new QLabel("Add or update investment");
        title->setObjectName("sectionTitle");
        category_name_ = new QLineEdit;
        category_amount_ = new QLineEdit;
        category_name_->setPlaceholderText("Bank FD, Bank Balance, Stocks");
        category_amount_->setPlaceholderText("369280");

        auto* form = new QFormLayout;
        form->addRow("Category", category_name_);
        form->addRow("Amount", category_amount_);

        category_replace_ = new QRadioButton("Set new amount (replace existing)");
        category_add_ = new QRadioButton("Add to existing amount");
        category_replace_->setChecked(true);
        auto* mode_group = new QButtonGroup(this);
        mode_group->addButton(category_replace_);
        mode_group->addButton(category_add_);
        form_layout->addWidget(title);
        form_layout->addLayout(form);
        form_layout->addWidget(category_replace_);
        form_layout->addWidget(category_add_);
        auto* save_button = new QPushButton("Save category");
        form_layout->addWidget(save_button);
        form_layout->addStretch(1);
        connect(save_button, &QPushButton::clicked, this, [this] { save_category(); });

        auto* hint = card();
        auto* hint_layout = new QVBoxLayout(hint);
        auto* hint_title = new QLabel("Data entry");
        hint_title->setObjectName("sectionTitle");
        auto* hint_text = new QLabel("Amounts are stored exactly as integer cents/paise internally. You can enter values like 369280 or 369280.50.");
        hint_text->setWordWrap(true);
        hint_text->setObjectName("muted");
        hint_layout->addWidget(hint_title);
        hint_layout->addWidget(hint_text);
        hint_layout->addStretch(1);

        root->addWidget(form_card, 1);
        root->addWidget(hint, 1);
        tabs_->addTab(page, "Category");
    }

    void build_earmark_tab() {
        auto* page = new QWidget;
        auto* root = new QHBoxLayout(page);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(18);

        auto* form_card = card();
        auto* form_layout = new QVBoxLayout(form_card);
        auto* title = new QLabel("Allocate earmarks");
        title->setObjectName("sectionTitle");
        earmark_category_ = new QComboBox;
        available_label_ = new QLabel;
        available_label_->setObjectName("muted");
        earmark_purpose_ = new QLineEdit;
        earmark_amount_ = new QLineEdit;
        earmark_purpose_->setPlaceholderText("Vacation, phone, emergency fund");
        earmark_amount_->setPlaceholderText("341620");

        auto* form = new QFormLayout;
        form->addRow("Investment", earmark_category_);
        form->addRow("", available_label_);
        form->addRow("Purpose", earmark_purpose_);
        form->addRow("Amount", earmark_amount_);

        auto* save_button = new QPushButton("Save earmark");
        form_layout->addWidget(title);
        form_layout->addLayout(form);
        form_layout->addWidget(save_button);
        form_layout->addStretch(1);

        auto* chart_card = card();
        auto* chart_layout = new QVBoxLayout(chart_card);
        auto* chart_title = new QLabel("Allocation by investment");
        chart_title->setObjectName("sectionTitle");
        allocation_chart_ = new AllocationChart;
        allocation_chart_->on_edit = [this] { save(); refresh_all(); };
        auto* scroll_area = new QScrollArea;
        scroll_area->setWidget(allocation_chart_);
        scroll_area->setWidgetResizable(true);
        scroll_area->setFrameShape(QFrame::NoFrame);
        chart_layout->addWidget(chart_title);
        chart_layout->addWidget(scroll_area, 1);

        root->addWidget(form_card, 0);
        root->addWidget(chart_card, 1);

        connect(save_button, &QPushButton::clicked, this, [this] { save_earmark(); });
        connect(earmark_category_, &QComboBox::currentIndexChanged, this, [this] { refresh_available(); });

        tabs_->addTab(page, "Earmark");
    }

    QFrame* card() {
        auto* frame = new QFrame;
        frame->setObjectName("card");
        frame->setFrameShape(QFrame::NoFrame);
        return frame;
    }

    void apply_style() {
        qApp->setStyleSheet(R"(
            QMainWindow, QWidget { background: #f1f5f9; color: #0f172a; font-size: 14px; }
            QTabWidget::pane { border: 0; }
            QTabBar::tab { background: #e2e8f0; padding: 10px 18px; margin-right: 4px; border-top-left-radius: 6px; border-top-right-radius: 6px; }
            QTabBar::tab:selected { background: #ffffff; color: #1d4ed8; }
            QFrame#card { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; }
            QLabel#sectionTitle { font-size: 18px; font-weight: 700; color: #0f172a; }
            QLabel#total { font-size: 32px; font-weight: 800; color: #0f172a; }
            QLabel#muted { color: #64748b; }
            QLineEdit, QComboBox { background: #ffffff; border: 1px solid #cbd5e1; border-radius: 6px; padding: 8px; min-height: 22px; }
            QLineEdit:focus, QComboBox:focus { border-color: #2563eb; }
            QPushButton { background: #2563eb; color: white; border: 0; border-radius: 6px; padding: 9px 14px; font-weight: 600; }
            QPushButton:hover { background: #1d4ed8; }
            QPushButton#danger { background: #dc2626; }
            QPushButton#danger:hover { background: #b91c1c; }
            QHeaderView::section { background: #f8fafc; border: 0; padding: 7px; color: #475569; font-weight: 700; }
            QTableWidget { background: #ffffff; border: 0; gridline-color: #e2e8f0; }
        )");
    }

    void refresh_all() {
        total_label_->setText(fmt(portfolio_.total_cents(), privacy_hidden_));
        if (privacy_btn_) privacy_btn_->setText(privacy_hidden_
            ? QString::fromUtf8("\xF0\x9F\x91\x81\xE2\x80\x8D\xE2\x97\xA8\xEF\xB8\x8F")
            : QString::fromUtf8("\xF0\x9F\x91\x81"));
        pie_chart_->set_portfolio(&portfolio_, privacy_hidden_);
        allocation_chart_->set_portfolio(&portfolio_, privacy_hidden_);
        refresh_table();
        refresh_combo();
        refresh_available();
    }

    void refresh_table() {
        const auto slices = portfolio_.distribution();
        category_table_->setRowCount(static_cast<int>(slices.size()));
        for (int row = 0; row < static_cast<int>(slices.size()); ++row) {
            category_table_->setItem(row, 0, new QTableWidgetItem(qstr(slices[row].label)));
            category_table_->setItem(row, 1, new QTableWidgetItem(fmt(slices[row].amount_cents, privacy_hidden_)));
            category_table_->setItem(row, 2, new QTableWidgetItem(QString::number(slices[row].percentage, 'f', 1) + "%"));
        }
    }

    void refresh_combo() {
        const QString current = earmark_category_->currentText();
        earmark_category_->blockSignals(true);
        earmark_category_->clear();
        for (const auto& asset : portfolio_.assets()) {
            earmark_category_->addItem(qstr(asset.name));
        }
        const int existing = earmark_category_->findText(current);
        if (existing >= 0) {
            earmark_category_->setCurrentIndex(existing);
        }
        earmark_category_->blockSignals(false);
    }

    void refresh_available() {
        const auto* asset = selected_asset();
        if (asset == nullptr) {
            available_label_->setText("Add an investment category first.");
            return;
        }
        const finance::Money earmarked = portfolio_.earmarked_total_cents(asset->name);
        const finance::Money available = asset->amount_cents - earmarked;
        available_label_->setText(qstr("Available: ") + fmt(available, privacy_hidden_) + qstr(" of ") + fmt(asset->amount_cents, privacy_hidden_));
    }

    void toggle_privacy() {
        privacy_hidden_ = !privacy_hidden_;
        refresh_all();
    }

    const finance::Asset* selected_asset() const {
        return portfolio_.find_asset(str(earmark_category_->currentText()));
    }

    void save_category() {
        const auto amount = finance::parse_money(str(category_amount_->text()));
        if (!amount) {
            QMessageBox::warning(this, "Could not save", "Check the category name, amount, and existing earmarks.");
            return;
        }

        const std::string name = str(category_name_->text());
        if (category_add_->isChecked() && portfolio_.find_asset(name)) {
            const auto* existing = portfolio_.find_asset(name);
            const finance::Money total = existing->amount_cents + *amount;
            if (!portfolio_.upsert_asset(name, total)) {
                QMessageBox::warning(this, "Could not save", "Check the category name, amount, and existing earmarks.");
                return;
            }
        } else if (!portfolio_.upsert_asset(name, *amount)) {
            QMessageBox::warning(this, "Could not save", "Check the category name, amount, and existing earmarks.");
            return;
        }

        save();
        category_name_->clear();
        category_amount_->clear();
        refresh_all();
        tabs_->setCurrentIndex(0);
    }

    void save_earmark() {
        const auto* asset = selected_asset();
        if (asset == nullptr) {
            QMessageBox::warning(this, "No category", "Add an investment category before creating earmarks.");
            return;
        }

        const auto amount = finance::parse_money(str(earmark_amount_->text()));
        if (!amount) {
            QMessageBox::warning(this, "Invalid amount", "Enter a valid amount such as 341620 or 341620.50.");
            return;
        }

        if (!portfolio_.upsert_earmark(asset->name, str(earmark_purpose_->text()), *amount)) {
            const finance::Money available = asset->amount_cents - portfolio_.earmarked_total_cents(asset->name);
            QMessageBox::warning(this, "Could not allocate",
                qstr("This allocation is not valid. Available unallocated amount is " + finance::format_money(available) + "."));
            return;
        }

        save();
        earmark_purpose_->clear();
        earmark_amount_->clear();
        refresh_all();
    }

    void edit_category(const std::string& name) {
        const auto* asset = portfolio_.find_asset(name);
        if (!asset) return;
        category_name_->setText(qstr(asset->name));
        category_amount_->setText(fmt(asset->amount_cents, privacy_hidden_));
        category_replace_->setChecked(true);
        tabs_->setCurrentIndex(1);
    }

    void save() {
        finance::save_portfolio(portfolio_, kDataFile);
        finance::write_pie_chart_svg(portfolio_, kChartFile);
    }

    std::filesystem::path backup_path() const {
        const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
        return std::filesystem::path(kBackupDir) / ("portfolio_backup_" + stamp.toStdString() + ".txt");
    }

    bool export_backup(bool notify) {
        std::filesystem::create_directories(kBackupDir);
        const auto path = backup_path();
        if (!finance::save_portfolio(portfolio_, path)) {
            QMessageBox::warning(this, "Export failed", "Could not write the backup file.");
            return false;
        }
        if (notify) {
            QMessageBox::information(this, "Exported", qstr("Backup saved to " + path.string()));
        }
        return true;
    }

    void clear_all_entries() {
        const auto result = QMessageBox::question(this, "Clear all entries",
            "Export the current database and clear all entries from the app?",
            QMessageBox::Yes | QMessageBox::No);
        if (result != QMessageBox::Yes) {
            return;
        }
        if (!export_backup(false)) {
            return;
        }
        portfolio_ = finance::Portfolio{};
        save();
        refresh_all();
        QMessageBox::information(this, "Cleared", "A backup was exported, then the active database was cleared.");
    }

    void import_latest_backup() {
        if (!std::filesystem::exists(kBackupDir)) {
            QMessageBox::information(this, "No backup", "No backups found in data/backups.");
            return;
        }

        std::filesystem::path latest;
        std::filesystem::file_time_type latest_time{};
        for (const auto& entry : std::filesystem::directory_iterator(kBackupDir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".txt") {
                continue;
            }
            const auto time = entry.last_write_time();
            if (latest.empty() || time > latest_time) {
                latest = entry.path();
                latest_time = time;
            }
        }
        if (latest.empty()) {
            QMessageBox::information(this, "No backup", "No backups found in data/backups.");
            return;
        }

        const auto loaded = finance::load_portfolio(latest);
        if (!loaded) {
            QMessageBox::warning(this, "Import failed", "The latest backup could not be read.");
            return;
        }
        portfolio_ = *loaded;
        save();
        refresh_all();
        QMessageBox::information(this, "Imported", qstr("Imported " + latest.filename().string()));
    }
};

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
