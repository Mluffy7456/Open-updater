#include "openupdater/core/github.hpp"
#include "openupdater/core/updater.hpp"
#include "openupdater/core/version.hpp"

#include <QApplication>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <filesystem>
#include <stdexcept>

namespace {

constexpr auto kVersion = "2.1.0";

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QString("OpenUpdater %1").arg(kVersion));
        setWindowIcon(QIcon(":/icons/openupdater.svg"));
        resize(820, 620);
        setMinimumSize(700, 520);

        auto* central = new QWidget(this);
        auto* root = new QVBoxLayout(central);
        root->setContentsMargins(28, 24, 28, 24);
        root->setSpacing(16);

        auto* title = new QLabel("OpenUpdater", central);
        title->setObjectName("title");
        auto* subtitle = new QLabel(
            QString("Secure application updates · v%1").arg(kVersion), central);
        subtitle->setObjectName("subtitle");

        root->addWidget(title);
        root->addWidget(subtitle);

        auto* source_box = new QGroupBox("Update source", central);
        auto* form = new QFormLayout(source_box);
        form->setContentsMargins(18, 18, 18, 18);
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(12);

        current_ = new QLineEdit("1.0.0", source_box);
        repository_ = new QLineEdit("owner/repository", source_box);
        asset_ = new QLineEdit("DemoApp-windows-x64.zip", source_box);
        destination_ = new QLineEdit("./updates", source_box);
        digest_ = new QLineEdit(source_box);
        digest_->setPlaceholderText("Optional SHA-256 digest");

        form->addRow("Current version", current_);
        form->addRow("GitHub repository", repository_);
        form->addRow("Release asset", asset_);
        form->addRow("Install destination", destination_);
        form->addRow("SHA-256", digest_);

        root->addWidget(source_box);

        auto* actions = new QHBoxLayout();
        actions->setSpacing(10);
        check_button_ = new QPushButton("Check for update", central);
        update_button_ = new QPushButton("Install update", central);
        update_button_->setObjectName("primary");
        actions->addWidget(check_button_);
        actions->addWidget(update_button_);
        root->addLayout(actions);

        auto* status_box = new QGroupBox("Status", central);
        auto* status_layout = new QVBoxLayout(status_box);
        status_layout->setContentsMargins(18, 18, 18, 18);

        status_ = new QLabel("Ready. Configure the source and check for an update.", status_box);
        status_->setWordWrap(true);
        status_->setMinimumHeight(54);

        progress_ = new QProgressBar(status_box);
        progress_->setRange(0, 100);
        progress_->setValue(0);
        progress_->setTextVisible(false);
        progress_->setVisible(false);

        status_layout->addWidget(status_);
        status_layout->addWidget(progress_);
        root->addWidget(status_box);
        root->addStretch();

        setCentralWidget(central);

        connect(check_button_, &QPushButton::clicked, this, [this] {
            run_async("Checking the latest GitHub release...", [this] {
                const auto current = parse_current_version();
                const auto release = openupdater::GitHubReleasesProvider::latest(
                    repository_->text().toStdString(),
                    asset_->text().toStdString());

                if (release.version > current) {
                    return QString("Update available: %1 → %2")
                        .arg(QString::fromStdString(current.str()))
                        .arg(QString::fromStdString(release.version.str()));
                }

                return QString("Already up to date: %1")
                    .arg(QString::fromStdString(current.str()));
            });
        });

        connect(update_button_, &QPushButton::clicked, this, [this] {
            run_async("Downloading and installing the update...", [this] {
                const auto current = parse_current_version();
                const auto result = openupdater::Updater::update_from_github(
                    current,
                    repository_->text().toStdString(),
                    asset_->text().toStdString(),
                    std::filesystem::path(destination_->text().toStdString()),
                    digest_->text().toStdString());

                if (result.state == openupdater::UpdateState::UpToDate) {
                    return QString("No update required. Current version: %1")
                        .arg(QString::fromStdString(result.available.str()));
                }

                QString message = QString("Updated successfully: %1 → %2")
                    .arg(QString::fromStdString(result.current.str()))
                    .arg(QString::fromStdString(result.available.str()));

                if (!result.backup.empty()) {
                    message += QString("\nBackup: %1")
                        .arg(QString::fromStdString(result.backup.string()));
                }

                return message;
            });
        });
    }

private:
    [[nodiscard]] openupdater::Version parse_current_version() const {
        const auto current = openupdater::Version(current_->text().trimmed().toStdString());
        if (!current.valid())
            throw std::runtime_error("Invalid current version.");
        return current;
    }

    template <typename Function>
    void run_async(const QString& message, Function function) {
        check_button_->setEnabled(false);
        update_button_->setEnabled(false);
        progress_->setVisible(true);
        progress_->setRange(0, 0);
        status_->setText(message);

        auto* watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
            progress_->setRange(0, 100);
            progress_->setValue(100);
            progress_->setVisible(false);
            status_->setText(watcher->result());
            check_button_->setEnabled(true);
            update_button_->setEnabled(true);
            watcher->deleteLater();
        });

        watcher->setFuture(QtConcurrent::run([function]() {
            try {
                return function();
            } catch (const std::exception& error) {
                return QString("Error: %1").arg(QString::fromStdString(error.what()));
            }
        }));
    }

    QLineEdit* current_{};
    QLineEdit* repository_{};
    QLineEdit* asset_{};
    QLineEdit* destination_{};
    QLineEdit* digest_{};
    QPushButton* check_button_{};
    QPushButton* update_button_{};
    QLabel* status_{};
    QProgressBar* progress_{};
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
        QLabel#subtitle { color: #a7a1b3; font-size: 11pt; }
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
        QLineEdit {
            min-height: 34px;
            border: 1px solid #332b40;
            border-radius: 8px;
            padding: 0 10px;
            background: #0d0d13;
            color: #ffffff;
            selection-background-color: #a855f7;
        }
        QLineEdit:focus { border: 1px solid #c026d3; }
        QPushButton {
            min-height: 42px;
            border: 1px solid #3a3048;
            border-radius: 9px;
            padding: 0 20px;
            background: #17131d;
            color: #f4f1f8;
            font-weight: 600;
        }
        QPushButton:hover { background: #211a29; border-color: #7c3aed; }
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
        QProgressBar::chunk { border-radius: 3px; background: #c026d3; }
    )");

    MainWindow window;
    window.show();
    return application.exec();
}
