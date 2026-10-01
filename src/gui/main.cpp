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
#include <QMessageBox>
#include <QProgressBar>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <filesystem>
#include <optional>
#include <stdexcept>

namespace {

constexpr auto kVersion = openupdater::API_VERSION;
constexpr auto kRepository = "Mluffy7456/Open-updater";
constexpr auto kComponent = "OpenUpdater";

struct CheckResult {
    openupdater::Version current;
    openupdater::GitHubRelease release;
};

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QString("OpenUpdater %1").arg(kVersion));
        setWindowIcon(QIcon(":/icons/openupdater.svg"));
        resize(760, 520);
        setMinimumSize(680, 470);

        auto* central = new QWidget(this);
        auto* root = new QVBoxLayout(central);
        root->setContentsMargins(32, 28, 32, 28);
        root->setSpacing(16);

        auto* title = new QLabel("OpenUpdater", central);
        title->setObjectName("title");
        auto* subtitle = new QLabel(
            QString("Secure application updates · v%1").arg(kVersion), central);
        subtitle->setObjectName("subtitle");
        root->addWidget(title);
        root->addWidget(subtitle);

        auto* app_box = new QGroupBox("Application", central);
        auto* app_layout = new QVBoxLayout(app_box);
        app_layout->setContentsMargins(18, 18, 18, 18);

        version_ = new QLabel(QString("Current version  %1").arg(kVersion), app_box);
        version_->setObjectName("version");
        source_ = new QLabel(
            QString("Update source  ·  GitHub / %1").arg(kRepository), app_box);
        source_->setObjectName("source");
        app_layout->addWidget(version_);
        app_layout->addWidget(source_);
        root->addWidget(app_box);

        auto* actions = new QHBoxLayout();
        actions->setSpacing(10);
        check_button_ = new QPushButton("Check for updates", central);
        update_button_ = new QPushButton("Install update", central);
        update_button_->setObjectName("primary");
        update_button_->setEnabled(false);
        actions->addWidget(check_button_);
        actions->addWidget(update_button_);
        root->addLayout(actions);

        auto* status_box = new QGroupBox("Status", central);
        auto* status_layout = new QVBoxLayout(status_box);
        status_layout->setContentsMargins(18, 18, 18, 18);

        status_ = new QLabel(
            "Ready. Check GitHub for the latest version.", status_box);
        status_->setWordWrap(true);
        status_->setMinimumHeight(64);

        progress_ = new QProgressBar(status_box);
        progress_->setRange(0, 0);
        progress_->setTextVisible(false);
        progress_->setVisible(false);

        status_layout->addWidget(status_);
        status_layout->addWidget(progress_);
        root->addWidget(status_box);
        root->addStretch();
        setCentralWidget(central);

        connect(check_button_, &QPushButton::clicked, this, [this] {
            check_for_update();
        });
        connect(update_button_, &QPushButton::clicked, this, [this] {
            install_update();
        });
    }

private:
    void check_for_update() {
        available_release_.reset();
        check_button_->setEnabled(false);
        update_button_->setEnabled(false);
        progress_->setVisible(true);
        status_->setText("Checking GitHub for the latest release...");

        auto* watcher = new QFutureWatcher<CheckResult>(this);
        connect(watcher, &QFutureWatcher<CheckResult>::finished,
                this, [this, watcher] {
            try {
                const auto result = watcher->result();
                progress_->setVisible(false);
                check_button_->setEnabled(true);

                if (result.release.version > result.current) {
                    available_release_ = result.release;
                    status_->setText(
                        QString("Update available\n%1  →  %2\n%3")
                            .arg(QString::fromStdString(result.current.str()))
                            .arg(QString::fromStdString(result.release.version.str()))
                            .arg(QString::fromStdString(result.release.asset)));
                    update_button_->setEnabled(true);
                } else {
                    status_->setText(
                        QString("You're up to date.\nCurrent version: %1")
                            .arg(QString::fromStdString(result.current.str())));
                }
            } catch (const std::exception& error) {
                progress_->setVisible(false);
                check_button_->setEnabled(true);
                status_->setText(
                    QString("Error: %1").arg(QString::fromStdString(error.what())));
            }
            watcher->deleteLater();
        });

        watcher->setFuture(QtConcurrent::run([] {
            const openupdater::Version current(kVersion);
            if (!current.valid())
                throw std::runtime_error("Application version is invalid.");

            const auto release =
                openupdater::GitHubReleasesProvider::latest_compatible(
                    kRepository, kComponent);

            return CheckResult{current, release};
        }));
    }

    void install_update() {
        if (!available_release_)
            return;

        const auto release = *available_release_;
        const auto answer = QMessageBox::question(
            this,
            "Install update",
            QString("Download and install OpenUpdater %1?\n\nAsset: %2")
                .arg(QString::fromStdString(release.version.str()))
                .arg(QString::fromStdString(release.asset)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (answer != QMessageBox::Yes)
            return;

        check_button_->setEnabled(false);
        update_button_->setEnabled(false);
        progress_->setVisible(true);
        status_->setText("Downloading and verifying the update...");

        auto* watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished,
                this, [this, watcher] {
            try {
                status_->setText(watcher->result());
                progress_->setVisible(false);
                QTimer::singleShot(700, qApp, &QApplication::quit);
            } catch (const std::exception& error) {
                progress_->setVisible(false);
                check_button_->setEnabled(true);
                status_->setText(
                    QString("Error: %1").arg(QString::fromStdString(error.what())));
            }
            watcher->deleteLater();
        });

        watcher->setFuture(QtConcurrent::run([release] {
            const auto temp_dir =
                std::filesystem::path(
                    QStandardPaths::writableLocation(
                        QStandardPaths::TempLocation).toStdString()) /
                "OpenUpdater";
            std::filesystem::create_directories(temp_dir);

            const auto installer = temp_dir / release.asset;
            openupdater::Downloader::download(release.download_url, installer);

            if (!release.sha256.empty() &&
                !openupdater::verify_sha256(installer, release.sha256)) {
                std::error_code error;
                std::filesystem::remove(installer, error);
                throw std::runtime_error(
                    "SHA-256 verification failed for the downloaded installer.");
            }

            if (!QProcess::startDetached(
                    QString::fromStdString(installer.string()), {})) {
                throw std::runtime_error("Failed to start the downloaded installer.");
            }

            return QString("Update downloaded and installer started. OpenUpdater will close.");
        }));
    }

    QLabel* version_{};
    QLabel* source_{};
    QPushButton* check_button_{};
    QPushButton* update_button_{};
    QLabel* status_{};
    QProgressBar* progress_{};
    std::optional<openupdater::GitHubRelease> available_release_;
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
