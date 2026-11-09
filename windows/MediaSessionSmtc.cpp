// SystemMediaTransportControls qua WinRT ABI thuần (không cần C++/WinRT hay WRL) -> biên dịch được bằng MinGW-w64
// nếu toolchain có đủ header (windows.media.h + systemmediatransportcontrolsinterop.h). CMake tự thử biên dịch tệp này
// và chuyển sang MediaSessionStub.cpp nếu không được.
#include "MediaSession.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.media.h>
#include <systemmediatransportcontrolsinterop.h>

#include <atomic>
#include <utility>

using namespace ABI::Windows::Media;

namespace {

// Tên phẳng của ITypedEventHandler<SystemMediaTransportControls*, SystemMediaTransportControlsButtonPressedEventArgs*>
typedef __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CSystemMediaTransportControlsButtonPressedEventArgs ButtonHandler;

// IAgileObject: báo cho WinRT biết delegate này gọi được từ mọi thread (không cần marshal)
const GUID kIidAgile = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};

class ButtonHandlerImpl : public ButtonHandler {
public:
    explicit ButtonHandlerImpl(MediaCallbacks cb) : cb_(std::move(cb)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == kIidAgile || riid == __uuidof(ButtonHandler)) {
            *ppv = static_cast<ButtonHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG r = --ref_;
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ISystemMediaTransportControls*,
                                     ISystemMediaTransportControlsButtonPressedEventArgs* args) override {
        SystemMediaTransportControlsButton b;
        if (args && SUCCEEDED(args->get_Button(&b))) {
            switch (b) {
            case SystemMediaTransportControlsButton_Play:     if (cb_.play)  cb_.play();  break;
            case SystemMediaTransportControlsButton_Pause:    if (cb_.pause) cb_.pause(); break;
            case SystemMediaTransportControlsButton_Next:     if (cb_.next)  cb_.next();  break;
            case SystemMediaTransportControlsButton_Previous: if (cb_.prev)  cb_.prev();  break;
            default: break;
            }
        }
        return S_OK;
    }

private:
    std::atomic<ULONG> ref_{1};
    MediaCallbacks cb_;
};

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), &w[0], n);
    return w;
}

} // namespace

struct MediaSession::Impl {
    ISystemMediaTransportControls* smtc = nullptr;
    ISystemMediaTransportControlsDisplayUpdater* updater = nullptr;
    IMusicDisplayProperties* music = nullptr;
    ButtonHandlerImpl* handler = nullptr;
    EventRegistrationToken token{};
    bool ok = false;
};

MediaSession::MediaSession() : d(new Impl) {}

MediaSession::~MediaSession() {
    if (d->smtc && d->ok) {
        d->smtc->remove_ButtonPressed(d->token);
        d->smtc->put_IsEnabled(false);
    }
    if (d->handler) d->handler->Release();
    if (d->music) d->music->Release();
    if (d->updater) d->updater->Release();
    if (d->smtc) d->smtc->Release();
}

bool MediaSession::available() const { return d->ok; }

bool MediaSession::init(void* hwnd, MediaCallbacks cb) {
    RoInitialize(RO_INIT_MULTITHREADED);               // Qt đã khởi tạo COM; lỗi "đổi chế độ" có thể bỏ qua

    static const wchar_t kClass[] = L"Windows.Media.SystemMediaTransportControls";
    HSTRING_HEADER header;
    HSTRING cls = nullptr;
    if (FAILED(WindowsCreateStringReference(kClass, UINT32(sizeof(kClass) / sizeof(wchar_t) - 1), &header, &cls))) return false;

    ISystemMediaTransportControlsInterop* interop = nullptr;
    if (FAILED(RoGetActivationFactory(cls, __uuidof(ISystemMediaTransportControlsInterop), reinterpret_cast<void**>(&interop))) || !interop)
        return false;
    const HRESULT hr = interop->GetForWindow(static_cast<HWND>(hwnd), __uuidof(ISystemMediaTransportControls),
                                             reinterpret_cast<void**>(&d->smtc));
    interop->Release();
    if (FAILED(hr) || !d->smtc) return false;

    d->smtc->put_IsEnabled(true);
    d->smtc->put_IsPlayEnabled(true);
    d->smtc->put_IsPauseEnabled(true);
    d->smtc->put_IsNextEnabled(true);
    d->smtc->put_IsPreviousEnabled(true);

    d->handler = new ButtonHandlerImpl(std::move(cb));
    if (FAILED(d->smtc->add_ButtonPressed(d->handler, &d->token))) return false;

    if (FAILED(d->smtc->get_DisplayUpdater(&d->updater)) || !d->updater) return false;
    d->updater->put_Type(MediaPlaybackType_Music);
    if (FAILED(d->updater->get_MusicProperties(&d->music)) || !d->music) return false;

    d->ok = true;
    return true;
}

void MediaSession::setMetadata(const std::string& title, const std::string& artist) {
    if (!d->ok) return;
    const std::wstring t = widen(title), a = widen(artist);
    HSTRING ht = nullptr, ha = nullptr;
    if (SUCCEEDED(WindowsCreateString(t.c_str(), UINT32(t.size()), &ht))) { d->music->put_Title(ht); WindowsDeleteString(ht); }
    if (SUCCEEDED(WindowsCreateString(a.c_str(), UINT32(a.size()), &ha))) { d->music->put_Artist(ha); WindowsDeleteString(ha); }
    d->updater->Update();
}

void MediaSession::setPlaybackState(int state) {
    if (!d->ok) return;
    d->smtc->put_PlaybackStatus(state == 2 ? MediaPlaybackStatus_Playing
                                : state == 1 ? MediaPlaybackStatus_Paused
                                             : MediaPlaybackStatus_Stopped);
}
