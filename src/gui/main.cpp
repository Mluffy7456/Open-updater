#include "openupdater/core/github.hpp"
#include "openupdater/core/updater.hpp"
#include "openupdater/core/version.hpp"

#include <QApplication>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <filesystem>
#include <stdexcept>

namespace {

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("OpenUpdater 1.1.0");
        resize(760, 520);

        auto* central = new QWidget(this);
        auto* root = new QVBoxLayout(central);

        auto* config_box = new QGroupBox("Update source", central);
        auto* form = new QFormLayout(config_box);

        current_ = new QLineEdit("1.0.0", config_box);
        repository_ = new QLineEdit("owner/repository", config_box);
        asset_ = new QLineEdit("DemoApp-windows-x64.zip", config_box);
        destination_ = new QLineEdit("./updates", config_box);
        digest_ = new QLineEdit(config_box);
        digest_->setPlaceholderText("Optional SHA-256");

        form->addRow("Current version:", current_);
        form->addRow("GitHub repository:", repository_);
        form->addRow("Release asset:", asset_);
        form->addRow("Destination:", destination_);
        form->addRow("SHA-256:", digest_);

        auto* buttons = new QHBoxLayout();
        check_button_ = new QPushButton("Check for update", central);
        update_button_ = new QPushButton("Update now", central);
        buttons->addWidget(check_button_);
        buttons->addWidget(update_button_);

        status_ = new QLabel("Ready.", central);
        status_->setWordWrap(true);

        root->addWidget(config_box);
        root->addLayout(buttons);
        root->addWidget(status_);
        root->addStretch();

        setCentralWidget(central);

        connect(check_button_, &QPushButton::clicked, this, [this] {
            run_async("Checking GitHub release...", [this] {
                const auto current = openupdater::Version(current_->text().toStdString());
                if (!current.valid())
                    throw std::runtime_error("Invalid current version.");

                const auto release = openupdater::GitHubReleasesProvider::latest(
                    repository_->text().toStdString(),
                    asset_->text().toStdString());

                if (release.version > current) {
                    return QString("Update available: %1 -> %2")
                        .arg(QString::fromStdString(current.str()))
                        .arg(QString::fromStdString(release.version.str()));
                }

                return QString("Already up to date: %1")
                    .arg(QString::fromStdString(current.str()));
            });
        });

        connect(update_button_, &QPushButton::clicked, this, [this] {
            run_async("Installing update...", [this] {
                const auto current = openupdater::Version(current_->text().toStdString());
                if (!current.valid())
                    throw std::runtime_error("Invalid current version.");

                const auto result = openupdater::Updater::update_from_github(
                    current,
                    repository_->text().toStdString(),
                    asset_->text().toStdString(),
                    std::filesystem::path(destination_->text().toStdString()),
                    digest_->text().toStdString());

                if (result.state == openupdater::UpdateState::UpToDate)
                    return QString("No update required. Current version: %1")
                        .arg(QString::fromStdString(result.available.str()));

                QString message = QString("Updated: %1 -> %2")
                    .arg(QString::fromStdString(result.current.str()))
                    .arg(QString::fromStdString(result.available.str()));

                if (!result.backup.empty())
                    message += QString("\nBackup: %1")
                        .arg(QString::fromStdString(result.backup.string()));

                return message;
            });
        });
    }

private:
    template <typename Function>
    void run_async(const QString& message, Function function) {
        check_button_->setEnabled(false);
        update_button_->setEnabled(false);
        status_->setText(message);

        auto* watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
            status_->setText(watcher->result());
            check_button_->setEnabled(true);
            update_button_->setEnabled(true);
            watcher->deleteLater();
        });

        watcher->setFuture(QtConcurrent::run([function]() {
            try {
                return function();
            } catch (const std::exception& error) {
                return QString("ERROR: %1").arg(QString::fromStdString(error.what()));
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
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    MainWindow window;
    window.show();
    return application.exec();
}
