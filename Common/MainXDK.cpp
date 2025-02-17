//
// Main.cpp
//

#include "pch.h"
#include "Game.h"

#include <ppltasks.h>

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::Foundation;
using namespace DirectX;

bool g_HDRMode = false;

ref class ViewProvider sealed : public IFrameworkView
{
public:
    ViewProvider() :
        m_exit(false)
    {
    }

    // IFrameworkView methods
    virtual void Initialize(CoreApplicationView^ applicationView)
    {
        applicationView->Activated +=
            ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &ViewProvider::OnActivated);

        CoreApplication::Suspending +=
            ref new EventHandler<SuspendingEventArgs^>(this, &ViewProvider::OnSuspending);

        CoreApplication::Resuming +=
            ref new EventHandler<Platform::Object^>(this, &ViewProvider::OnResuming);

        CoreApplication::DisableKinectGpuReservation = true;

        m_game = std::make_unique<Game>();
        if (m_game->RequestHDRMode())
        {
            // Request HDR mode.
            auto determineHDR = Concurrency::create_task(
                Windows::Xbox::Graphics::Display::DisplayConfiguration::TrySetHdrModeAsync()
            );

            // In a real game, you'd do some initialization here to hide the HDR mode switch.

            // Finish up HDR mode detection (waiting for async if needed)
            g_HDRMode = determineHDR.get()->HdrEnabled;

#ifdef _DEBUG
            OutputDebugStringA((g_HDRMode) ? "INFO: Display in HDR Mode\n" : "INFO: Display in SDR Mode\n");
#endif
        }
    }

    virtual void Uninitialize()
    {
        m_game.reset();
    }

    virtual void SetWindow(CoreWindow^ window)
    {
        window->Closed +=
            ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &ViewProvider::OnWindowClosed);

        m_game->Initialize(reinterpret_cast<IUnknown*>(window), 0, 0, DXGI_MODE_ROTATION_IDENTITY);
    }

    virtual void Load(Platform::String^ entryPoint)
    {
    }

    virtual void Run()
    {
        while (!m_exit)
        {
            m_game->Tick();

            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
        }
    }

protected:
    // Event handlers
    void OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
    {
        CoreWindow::GetForCurrentThread()->Activate();
    }

    void OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
    {
        auto deferral = args->SuspendingOperation->GetDeferral();

        create_task([this, deferral]()
        {
            m_game->OnSuspending();

            deferral->Complete();
        });
    }

    void OnResuming(Platform::Object^ sender, Platform::Object^ args)
    {
        m_game->OnResuming();
    }

    void OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
    {
        m_exit = true;
    }

private:
    bool                    m_exit;
    std::unique_ptr<Game>   m_game;
};

ref class ViewProviderFactory : IFrameworkViewSource
{
public:
    virtual IFrameworkView^ CreateView()
    {
        return ref new ViewProvider();
    }
};


// Entry point
[Platform::MTAThread]
int __cdecl main(Platform::Array<Platform::String^>^ /*argv*/)
{
    auto viewProviderFactory = ref new ViewProviderFactory();
    CoreApplication::Run(viewProviderFactory);
    return 0;
}


// Exit helper
void ExitGame() noexcept
{
    Windows::ApplicationModel::Core::CoreApplication::Exit();
}