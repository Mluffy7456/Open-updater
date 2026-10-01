#include "openupdater/providers/windows_update.hpp"

#ifdef _WIN32

#include <windows.h>
#include <wuapi.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#pragma comment(lib, "wuapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace openupdater {

namespace {

class ComScope {
public:
    ComScope() {
        const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(result) && result != RPC_E_CHANGED_MODE)
            throw std::runtime_error("CoInitializeEx failed.");
        initialized_ = SUCCEEDED(result);
    }

    ~ComScope() {
        if (initialized_)
            CoUninitialize();
    }

private:
    bool initialized_ = false;
};

template <typename T>
void release(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

std::string bstr_to_string(BSTR value) {
    if (!value)
        return {};

    const int length = WideCharToMultiByte(
        CP_UTF8, 0, value, SysStringLen(value),
        nullptr, 0, nullptr, nullptr);

    if (length <= 0)
        return {};

    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value, SysStringLen(value),
        result.data(), length, nullptr, nullptr);
    return result;
}

std::string hresult_message(const char* operation, HRESULT result) {
    std::ostringstream message;
    message << operation << " failed (HRESULT 0x"
            << std::hex << std::uppercase
            << static_cast<unsigned long>(result) << ").";
    return message.str();
}

std::string driver_date(DATE date) {
    SYSTEMTIME system_time{};
    if (!VariantTimeToSystemTime(date, &system_time))
        return "Windows Update catalog";

    std::ostringstream result;
    result << std::setfill('0')
           << std::setw(4) << system_time.wYear << '-'
           << std::setw(2) << system_time.wMonth << '-'
           << std::setw(2) << system_time.wDay;
    return result.str();
}

IUpdateCollection* search_driver_updates(
    IUpdateSession* session,
    const std::string& id = {}) {

    IUpdateSearcher* searcher = nullptr;
    HRESULT result = session->CreateUpdateSearcher(&searcher);
    if (FAILED(result))
        throw std::runtime_error(hresult_message(
            "CreateUpdateSearcher", result));

    std::wstring criteria =
        L"IsInstalled=0 and IsHidden=0 and Type='Driver'";

    if (!id.empty()) {
        criteria =
            L"IsInstalled=0 and IsHidden=0 and Type='Driver' and UpdateID='" +
            std::wstring(id.begin(), id.end()) + L"'";
    }

    BSTR criteria_bstr = SysAllocString(criteria.c_str());
    if (!criteria_bstr) {
        release(searcher);
        throw std::runtime_error("Unable to allocate Windows Update search criteria.");
    }

    ISearchResult* search_result = nullptr;
    result = searcher->Search(criteria_bstr, &search_result);
    SysFreeString(criteria_bstr);
    release(searcher);

    if (FAILED(result)) {
        release(search_result);
        throw std::runtime_error(hresult_message(
            "Windows Update driver search", result));
    }

    IUpdateCollection* updates = nullptr;
    result = search_result->get_Updates(&updates);
    release(search_result);

    if (FAILED(result))
        throw std::runtime_error(hresult_message(
            "Reading Windows Update results", result));

    return updates;
}

} // namespace

std::string WindowsUpdateProvider::name() const {
    return "Windows Update";
}

bool WindowsUpdateProvider::available() const {
    return true;
}

std::vector<DiscoveredUpdate> WindowsUpdateProvider::check_updates() {
    ComScope com;

    IUpdateSession* session = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_UpdateSession,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUpdateSession,
        reinterpret_cast<void**>(&session));

    if (FAILED(result))
        throw std::runtime_error(hresult_message(
            "Create Windows Update session", result));

    IUpdateCollection* updates = nullptr;

    try {
        updates = search_driver_updates(session);
        LONG count = 0;
        result = updates->get_Count(&count);
        if (FAILED(result))
            throw std::runtime_error(hresult_message(
                "Read Windows Update count", result));

        std::vector<DiscoveredUpdate> result_updates;
        for (LONG index = 0; index < count; ++index) {
            IUpdate* update = nullptr;
            if (FAILED(updates->get_Item(index, &update)) || !update)
                continue;

            BSTR title = nullptr;
            IUpdateIdentity* identity = nullptr;
            IWindowsDriverUpdate* driver = nullptr;

            const auto title_result = update->get_Title(&title);
            const auto identity_result = update->get_Identity(&identity);
            const auto driver_result = update->QueryInterface(
                IID_IWindowsDriverUpdate,
                reinterpret_cast<void**>(&driver));

            if (SUCCEEDED(title_result) &&
                SUCCEEDED(identity_result) &&
                SUCCEEDED(driver_result)) {

                BSTR update_id = nullptr;
                BSTR provider = nullptr;
                BSTR model = nullptr;
                DATE version_date{};

                if (SUCCEEDED(identity->get_UpdateID(&update_id)) &&
                    SUCCEEDED(driver->get_DriverProvider(&provider)) &&
                    SUCCEEDED(driver->get_DriverModel(&model)) &&
                    SUCCEEDED(driver->get_DriverVerDate(&version_date))) {

                    VARIANT_BOOL reboot_required = VARIANT_FALSE;
                    IUpdate2* update2 = nullptr;
                    if (SUCCEEDED(update->QueryInterface(
                            IID_IUpdate2,
                            reinterpret_cast<void**>(&update2)))) {
                        update2->get_RebootRequired(&reboot_required);
                    }

                    DiscoveredUpdate item;
                    item.id = bstr_to_string(update_id);
                    item.name = bstr_to_string(title);
                    item.current_version = "Installed";
                    item.available_version = driver_date(version_date);
                    item.category = UpdateCategory::Driver;
                    item.source = UpdateSource::WindowsUpdate;
                    item.provider = name();
                    item.publisher = bstr_to_string(provider);
                    item.platform = "Windows";
                    item.architecture = "System";
                    item.requires_admin = true;
                    item.requires_restart = reboot_required == VARIANT_TRUE;

                    if (!bstr_to_string(model).empty())
                        item.name += " · " + bstr_to_string(model);

                    if (!item.id.empty())
                        result_updates.push_back(std::move(item));

                    release(update2);
                }

                SysFreeString(update_id);
                SysFreeString(provider);
                SysFreeString(model);
            }

            SysFreeString(title);
            release(identity);
            release(driver);
            release(update);
        }

        release(updates);
        release(session);
        return result_updates;
    } catch (...) {
        release(updates);
        release(session);
        throw;
    }
}

void WindowsUpdateProvider::install(const DiscoveredUpdate& update) {
    if (update.source != UpdateSource::WindowsUpdate ||
        update.provider != name() ||
        update.id.empty()) {
        throw std::invalid_argument(
            "Invalid Windows Update identifier.");
    }

    ComScope com;

    IUpdateSession* session = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_UpdateSession,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUpdateSession,
        reinterpret_cast<void**>(&session));

    if (FAILED(result))
        throw std::runtime_error(hresult_message(
            "Create Windows Update session", result));

    IUpdateCollection* updates = nullptr;
    try {
        updates = search_driver_updates(session, update.id);

        LONG count = 0;
        result = updates->get_Count(&count);
        if (FAILED(result))
            throw std::runtime_error(hresult_message(
                "Read Windows Update count", result));

        if (count == 0)
            throw std::runtime_error(
                "The selected driver update is no longer available.");

        IUpdateDownloader* downloader = nullptr;
        result = session->CreateUpdateDownloader(&downloader);
        if (FAILED(result))
            throw std::runtime_error(hresult_message(
                "Create Windows Update downloader", result));

        result = downloader->put_Updates(updates);
        if (FAILED(result)) {
            release(downloader);
            throw std::runtime_error(hresult_message(
                "Select Windows Update packages", result));
        }

        IDownloadResult* download_result = nullptr;
        result = downloader->Download(&download_result);
        release(downloader);

        if (FAILED(result)) {
            release(download_result);
            throw std::runtime_error(hresult_message(
                "Download Windows Update driver", result));
        }

        OperationResultCode download_code = orcFailed;
        if (FAILED(download_result->get_ResultCode(&download_code)) ||
            (download_code != orcSucceeded &&
             download_code != orcSucceededWithErrors)) {
            release(download_result);
            throw std::runtime_error(
                "Windows Update driver download did not succeed.");
        }
        release(download_result);

        IUpdateInstaller* installer = nullptr;
        result = session->CreateUpdateInstaller(&installer);
        if (FAILED(result))
            throw std::runtime_error(hresult_message(
                "Create Windows Update installer", result));

        result = installer->put_Updates(updates);
        if (FAILED(result)) {
            release(installer);
            throw std::runtime_error(hresult_message(
                "Select Windows Update packages for installation", result));
        }

        IInstallationResult* installation_result = nullptr;
        result = installer->Install(&installation_result);
        release(installer);

        if (FAILED(result)) {
            release(installation_result);
            throw std::runtime_error(hresult_message(
                "Install Windows Update driver", result));
        }

        OperationResultCode install_code = orcFailed;
        if (FAILED(installation_result->get_ResultCode(&install_code)) ||
            (install_code != orcSucceeded &&
             install_code != orcSucceededWithErrors)) {
            release(installation_result);
            throw std::runtime_error(
                "Windows Update driver installation did not succeed.");
        }

        release(installation_result);
        release(updates);
        release(session);
    } catch (...) {
        release(updates);
        release(session);
        throw;
    }
}

} // namespace openupdater

#else

#include <stdexcept>

namespace openupdater {

std::string WindowsUpdateProvider::name() const {
    return "Windows Update";
}

bool WindowsUpdateProvider::available() const {
    return false;
}

std::vector<DiscoveredUpdate> WindowsUpdateProvider::check_updates() {
    return {};
}

void WindowsUpdateProvider::install(const DiscoveredUpdate&) {
    throw std::runtime_error("Windows Update is only available on Windows.");
}

} // namespace openupdater

#endif
