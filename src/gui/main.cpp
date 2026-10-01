#include "openupdater/core/api.hpp"
#include "openupdater/core/universal.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr auto kVersion = openupdater::API_VERSION;

QString category_title(openupdater::UpdateCategory category) {
    return QString::fromUtf8(
        openupdater::update_category_name(category));
}

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QString("OpenUpdater %1").arg(kVersion));
        setWindowIcon(QIcon(":/icons/openupdater.svg"));
        resize(980, 720);
        setMinimumSize(820, 600);

        auto* central = new QWidget(this);
        auto* root = new QVBoxLayout(central);
        root->setContentsMargins(30, 26, 30, 26);
        root->setSpacing(14);

        auto* title = new QLabel("OpenUpdater", central);
        title->setObjectName("title");

        auto* subtitle = new QLabel(
            QString("Universal update manager · v%1").arg(kVersion),
            central);
        subtitle->setObjectName("subtitle");

        root->addWidget(title);
        root->addWidget(subtitle);

        auto* actions = new QHBoxLayout();
        actions->setSpacing(10);

        check_button_ = new QPushButton("Check for updates", central);
        update_button_ = new QPushButton("Update selected", central);
        update_button_->setObjectName("primary");
        update_button_->setEnabled(false);

        select_all_button_ = new QPushButton("Select all", central);
        deselect_all_button_ = new QPushButton("Deselect all", central);

        actions->addWidget(check_button_);
        actions->addWidget(select_all_button_);
        actions->addWidget(deselect_all_button_);
        actions->addStretch();
        actions->addWidget(update_button_);
        root->addLayout(actions);

        summary_ = new QLabel(
            "No scan performed yet.", central);
        summary_->setObjectName("summary");
        root->addWidget(summary_);

        auto* scroll = new QScrollArea(central);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        update_container_ = new QWidget(scroll);
        update_layout_ = new QVBoxLayout(update_container_);
        update_layout_->setContentsMargins(4, 4, 12, 4);
        update_layout_->setSpacing(12);
        update_layout_->addStretch();

        scroll->setWidget(update_container_);
        root->addWidget(scroll, 1);

        auto* status_box = new QGroupBox("Status", central);
        auto* status_layout = new QVBoxLayout(status_box);
        status_layout->setContentsMargins(16, 16, 16, 16);

        status_ = new QLabel(
            "Ready. OpenUpdater will check installed applications and Windows driver updates.",
            status_box);
        status_->setWordWrap(true);

        progress_ = new QProgressBar(status_box);
        progress_->setRange(0, 0);
        progress_->setTextVisible(false);
        progress_->setVisible(false);

        status_layout->addWidget(status_);
        status_layout->addWidget(progress_);
        root->addWidget(status_box);

        setCentralWidget(central);

        connect(check_button_, &QPushButton::clicked,
                this, [this] { check_for_updates(); });
        connect(update_button_, &QPushButton::clicked,
                this, [this] { install_selected(); });
        connect(select_all_button_, &QPushButton::clicked,
                this, [this] { set_all_checked(true); });
        connect(deselect_all_button_, &QPushButton::clicked,
                this, [this] { set_all_checked(false); });
    }

private:
    void clear_update_list() {
        while (auto* item = update_layout_->takeAt(0)) {
            if (auto* widget = item->widget())
                widget->deleteLater();
            delete item;
        }
        update_layout_->addStretch();
        checkboxes_.clear();
    }

    void render_updates() {
        clear_update_list();

        if (updates_.empty()) {
            auto* empty = new QLabel(
                "No updates are currently available from the supported providers.",
                update_container_);
            empty->setObjectName("empty");
            empty->setWordWrap(true);
            update_layout_->insertWidget(0, empty);
            return;
        }

        std::map<openupdater::UpdateCategory,
                 std::vector<std::size_t>> groups;

        for (std::size_t index = 0; index < updates_.size(); ++index)
            groups[updates_[index].category].push_back(index);

        for (const auto& [category, indices] : groups) {
            auto* box = new QGroupBox(category_title(category), update_container_);
            auto* layout = new QVBoxLayout(box);
            layout->setContentsMargins(14, 16, 14, 12);
            layout->setSpacing(8);

            for (const auto index : indices) {
                const auto& update = updates_[index];

                auto* check = new QCheckBox(box);
                check->setChecked(true);
                check->setText(
                    QString("%1    %2 → %3")
                        .arg(QString::fromStdString(update.name))
                        .arg(QString::fromStdString(update.current_version))
                        .arg(QString::fromStdString(update.available_version)));

                auto* meta = new QLabel(
                    QString("%1  ·  %2%3%4")
                        .arg(QString::fromStdString(
                            openupdater::update_source_name(update.source)))
                        .arg(QString::fromStdString(update.publisher.empty()
                            ? update.provider
                            : update.publisher))
                        .arg(update.requires_restart
                            ? "  ·  restart may be required"
                            : "")
                        .arg(update.requires_admin
                            ? "  ·  administrator approval may be required"
                            : ""),
                    box);
                meta->setObjectName("meta");
                meta->setIndent(24);

                layout->addWidget(check);
                layout->addWidget(meta);
                checkboxes_.push_back({index, check});
                connect(check, &QCheckBox::toggled,
                        this, [this] { update_selection_summary(); });
            }

            update_layout_->insertWidget(update_layout_->count() - 1, box);
        }

        update_selection_summary();
    }

    void update_selection_summary() {
        std::size_t selected = 0;
        for (const auto& row : checkboxes_) {
            if (row.checkbox->isChecked())
                ++selected;
        }

        summary_->setText(
            QString("%1 updates found · %2 selected")
                .arg(updates_.size())
                .arg(selected));
        update_button_->setEnabled(selected != 0 && !busy_);
    }

    void set_all_checked(bool checked) {
        for (const auto& row : checkboxes_)
            row.checkbox->setChecked(checked);
        update_selection_summary();
    }

    void set_busy(bool busy, const QString& message) {
        busy_ = busy;
        check_button_->setEnabled(!busy);
        select_all_button_->setEnabled(!busy);
        deselect_all_button_->setEnabled(!busy);
        progress_->setVisible(busy);
        status_->setText(message);

        if (busy) {
            update_button_->setEnabled(false);
        } else {
            update_selection_summary();
        }
    }

    void check_for_updates() {
        set_busy(true, "Scanning WinGet and Windows Update for available updates...");

        auto* watcher =
            new QFutureWatcher<openupdater::UpdateScanResult>(this);

        connect(
            watcher,
            &QFutureWatcher<openupdater::UpdateScanResult>::finished,
            this,
            [this, watcher] {
                try {
                    const auto result = watcher->result();
                    updates_ = result.updates;
                    render_updates();

                    QString message =
                        QString("Scan complete. Found %1 available update(s).")
                            .arg(updates_.size());

                    if (!result.provider_errors.empty()) {
                        message += "\n\nProvider diagnostics:";
                        for (const auto& error : result.provider_errors)
                            message += "\n• " +
                                QString::fromStdString(error);
                    }

                    set_busy(false, message);
                } catch (const std::exception& error) {
                    set_busy(false,
                        QString("Scan failed: %1")
                            .arg(QString::fromStdString(error.what())));
                } catch (...) {
                    set_busy(false, "Scan failed: unknown C++ exception.");
                }

                watcher->deleteLater();
            });

        watcher->setFuture(QtConcurrent::run([] {
            openupdater::UniversalUpdateManager manager;
            return manager.check_all();
        }));
    }

    std::vector<openupdater::DiscoveredUpdate> selected_updates() const {
        std::vector<openupdater::DiscoveredUpdate> selected;
        for (const auto& row : checkboxes_) {
            if (row.checkbox->isChecked())
                selected.push_back(updates_[row.index]);
        }
        return selected;
    }

    void install_selected() {
        const auto selected = selected_updates();
        if (selected.empty())
            return;

        const auto answer = QMessageBox::question(
            this,
            "Update selected",
            QString("Install %1 selected update(s)?\n\n"
                    "Some updates may require administrator approval or a restart.")
                .arg(selected.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (answer != QMessageBox::Yes)
            return;

        set_busy(true, "Installing selected updates...");

        auto* watcher =
            new QFutureWatcher<QString>(this);

        connect(
            watcher,
            &QFutureWatcher<QString>::finished,
            this,
            [this, watcher] {
                try {
                    set_busy(false, watcher->result());
                    for (auto& row : checkboxes_)
                        row.checkbox->setChecked(false);
                } catch (const std::exception& error) {
                    set_busy(false,
                        QString("Installation failed: %1")
                            .arg(QString::fromStdString(error.what())));
                } catch (...) {
                    set_busy(false, "Installation failed: unknown C++ exception.");
                }
                watcher->deleteLater();
            });

        watcher->setFuture(QtConcurrent::run(
            [selected] {
                openupdater::UniversalUpdateManager manager;
                std::size_t completed = 0;

                for (const auto& update : selected) {
                    manager.install(update);
                    ++completed;
                }

                return QString("Installation finished. %1 update(s) processed.")
                    .arg(completed);
            }));
    }

    struct CheckboxRow {
        std::size_t index;
        QCheckBox* checkbox;
    };

    QWidget* update_container_{};
    QVBoxLayout* update_layout_{};
    QLabel* summary_{};
    QLabel* status_{};
    QProgressBar* progress_{};
    QPushButton* check_button_{};
    QPushButton* update_button_{};
    QPushButton* select_all_button_{};
    QPushButton* deselect_all_button_{};

    std::vector<openupdater::DiscoveredUpdate> updates_;
    std::vector<CheckboxRow> checkboxes_;
    bool busy_ = false;
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setApplicationName("OpenUpdater");
    application.setApplicationVersion(kVersion);
    application.setWindowIcon(QIcon(":/icons/openupdater.svg"));

    application.setStyleSheet(R"(
        QWidget {
            background: #0b0b10;
            color: #f4f1f8;
            font-family: "Segoe UI";
            font-size: 10pt;
        }
        QMainWindow { background: #0b0b10; }
        QLabel#title {
            font-size: 28pt;
            font-weight: 700;
            color: #ffffff;
        }
        QLabel#subtitle {
            color: #a7a1b3;
            font-size: 11pt;
        }
        QLabel#summary {
            color: #d8b4fe;
            font-size: 11pt;
            font-weight: 600;
        }
        QLabel#meta {
            color: #8f889c;
            font-size: 9pt;
        }
        QLabel#empty {
            color: #8f889c;
            font-size: 11pt;
            padding: 30px;
        }
        QGroupBox {
            border: 1px solid #292332;
            border-radius: 12px;
            margin-top: 10px;
            padding-top: 12px;
            background: #111118;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 6px;
            color: #d8b4fe;
        }
        QScrollArea {
            border: 1px solid #292332;
            border-radius: 12px;
            background: #0e0e14;
        }
        QCheckBox {
            spacing: 9px;
            padding: 4px;
            font-weight: 600;
        }
        QCheckBox::indicator {
            width: 19px;
            height: 19px;
        }
        QPushButton {
            min-height: 42px;
            border: 1px solid #3a3048;
            border-radius: 9px;
            padding: 0 16px;
            background: #17131d;
            color: #f4f1f8;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #211a29;
            border-color: #7c3aed;
        }
        QPushButton:disabled { color: #6f6978; }
        QPushButton#primary {
            border: 1px solid #c026d3;
            background: #8b2bb5;
        }
        QPushButton#primary:hover { background: #a832c8; }
        QProgressBar {
            height: 6px;
            border: 0;
            border-radius: 3px;
            background: #211b29;
        }
        QProgressBar::chunk {
            border-radius: 3px;
            background: #c026d3;
        }
    )");

    MainWindow window;
    window.show();
    return application.exec();
}
