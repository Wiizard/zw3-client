#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Components/Modules/Remix/QueryPoll.hpp"
#include "Components/Modules/Remix/RenderFrameLock.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>

using namespace std::chrono_literals;
static Components::Remix::Device::RenderFrameLock uploadGate;
static thread_local unsigned uploadDepth = 0;
static std::atomic<unsigned> issues = 0, polls = 0;
static void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
namespace Game
{
    enum CriticalSection { };
    namespace Engine
    {
        enum ScopedCriticalSectionType { SCOPED_CRITSECT_NORMAL };
        struct ScopedCriticalSection
        {
            ScopedCriticalSection(CriticalSection section, ScopedCriticalSectionType type)
            {
                Require(int(section) == 0x19 && type == SCOPED_CRITSECT_NORMAL, "Wrong engine streaming lock");
                uploadGate.lock();
                ++uploadDepth;
            }
            ~ScopedCriticalSection() { --uploadDepth; uploadGate.unlock(); }
        };
    }
}
namespace Components::Remix::Device
{
    static RenderFrameLock renderFrameLock;
    static HRESULT STDMETHODCALLTYPE OriginalIssue(IDirect3DQuery9*, DWORD)
    {
        Require(uploadDepth > 0, "Issue raced the texture uploader");
        ++issues;
        return S_OK;
    }
    static HRESULT STDMETHODCALLTYPE OriginalGetData(IDirect3DQuery9*, void*, DWORD, DWORD)
    {
        Require(uploadDepth > 0 && renderFrameLock.OwnsCurrentThread(), "GetData raced an upload or render frame");
        ++polls;
        return S_OK;
    }
    static auto originalQueryIssue = &OriginalIssue;
    static auto originalQueryGetData = &OriginalGetData;
#include "QueryHooks.inc"
}
int main()
{
    try
    {
        using namespace Components::Remix::Device;
        std::promise<void> uploading, finishUpload;
        auto uploader = std::async(std::launch::async, [&]
        {
            const Game::Engine::ScopedCriticalSection native(static_cast<Game::CriticalSection>(0x19),
                Game::Engine::SCOPED_CRITSECT_NORMAL);
            uploading.set_value();
            finishUpload.get_future().wait();
            // Streaming can issue a query while holding its native lock. It must
            // never wait for the frame lock held by the rendering thread.
            return QueryIssueHook(nullptr, D3DISSUE_END);
        });
        uploading.get_future().wait();
        std::promise<void> frameStarted;
        auto renderer = std::async(std::launch::async, [&]
        {
            const std::lock_guard frame(renderFrameLock);
            frameStarted.set_value();
            return QueryGetDataHook(nullptr, nullptr, 0, D3DGETDATA_FLUSH);
        });
        frameStarted.get_future().wait();
        const bool frameWaited = renderer.wait_for(20ms) == std::future_status::timeout;
        // A foreign non-flushing poll must return promptly instead of waiting for
        // a render frame that is itself waiting on the uploader.
        const auto pending = QueryGetDataHook(nullptr, nullptr, 0, 0);
        finishUpload.set_value();
        const bool uploadFinished = uploader.wait_for(1s) == std::future_status::ready;
        const bool renderFinished = renderer.wait_for(1s) == std::future_status::ready;
        Require(uploadFinished && renderFinished, "Streaming/render query lock order deadlocked");
        Require(uploader.get() == S_OK && renderer.get() == S_OK && pending == S_FALSE && frameWaited,
            "Upload and query serialization failed");
        Require(issues == 1 && polls == 1, "Busy poll submitted commands or ready query was skipped");
        std::cout << "Production query hooks: native upload lock, recursive Issue, deferred poll and render/stream lock order passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
