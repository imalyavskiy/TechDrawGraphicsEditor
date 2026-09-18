#include "taskbarpin.h"

#include <QApplication>
#include <QSettings>

#ifdef Q_OS_WIN
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.h>

namespace {
/// Минимальные ABI-интерфейсы TaskbarManager. Они повторяют стабильный контракт Windows 10 и
/// позволяют Qt 5.15/MinGW 8.1 вызвать API без C++/WinRT, требующего отсутствующие корутины.
MIDL_INTERFACE("87490a19-1ad9-49f4-b2e8-86738dc5ac40") TaskbarManagerAbi : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_IsSupported(boolean *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsPinningAllowed(boolean *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsCurrentAppPinnedAsync(IInspectable **operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsAppListEntryPinnedAsync(IInspectable *entry, IInspectable **operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE RequestPinCurrentAppAsync(IInspectable **operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE RequestPinAppListEntryAsync(IInspectable *entry, IInspectable **operation) = 0;
};

MIDL_INTERFACE("db32ab74-de52-4fe6-b7b6-95ff9f8395df") TaskbarManagerStaticsAbi : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetDefault(TaskbarManagerAbi **result) = 0;
};

const IID taskbarDesktopSupportIidValue = {0xCDFEFD63, 0xE879, 0x4134, {0xB9, 0xA7, 0x82, 0x83, 0xF0, 0x5F, 0x94, 0x80}};
const IID taskbarStaticsIid = {0xDB32AB74, 0xDE52, 0x4FE6, {0xB7, 0xB6, 0x95, 0xFF, 0x9F, 0x83, 0x95, 0xDF}};
IInspectable *pendingPinOperation = nullptr;
} // namespace
#endif

/// Передаёт запрос системному TaskbarManager. Отсутствие API, запрет политикой и отказ пользователя
/// являются штатными исходами; сохранённый флаг снимается до запроса, чтобы окно не появлялось повторно.
void requestPendingTaskbarPin() {
    QSettings settings;
    if (!settings.value(QStringLiteral("installer/pendingTaskbarPin"), false).toBool())
        return;
    settings.remove(QStringLiteral("installer/pendingTaskbarPin"));

#ifdef Q_OS_WIN
    const HRESULT initialized = RoInitialize(RO_INIT_SINGLETHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        return;
    HSTRING className = nullptr;
    if (FAILED(WindowsCreateString(L"Windows.UI.Shell.TaskbarManager", 31, &className)))
        return;
    IInspectable *desktopSupport = nullptr;
    TaskbarManagerStaticsAbi *statics = nullptr;
    TaskbarManagerAbi *manager = nullptr;
    if (SUCCEEDED(RoGetActivationFactory(className, taskbarDesktopSupportIidValue,
                                         reinterpret_cast<void **>(&desktopSupport))) &&
        SUCCEEDED(RoGetActivationFactory(className, taskbarStaticsIid, reinterpret_cast<void **>(&statics))) &&
        SUCCEEDED(statics->GetDefault(&manager))) {
        boolean supported = false;
        boolean allowed = false;
        if (SUCCEEDED(manager->get_IsSupported(&supported)) && supported &&
            SUCCEEDED(manager->get_IsPinningAllowed(&allowed)) && allowed) {
            if (pendingPinOperation)
                pendingPinOperation->Release();
            pendingPinOperation = nullptr;
            manager->RequestPinCurrentAppAsync(&pendingPinOperation);
        }
    }
    if (manager)
        manager->Release();
    if (statics)
        statics->Release();
    if (desktopSupport)
        desktopSupport->Release();
    WindowsDeleteString(className);
#endif
}
