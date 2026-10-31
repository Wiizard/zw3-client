#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Components/Modules/Remix/QueryPoll.hpp"
#include "Components/Modules/Remix/RenderFrameLock.hpp"

#include <future>
#include <iostream>
#include <stdexcept>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void Check(DWORD flags, bool owner, bool canAcquire, HRESULT status)
    {
        unsigned polls = 0, acquires = 0, releases = 0;
        bool locked = false;
        const auto result = Components::Remix::Device::PollQuery(flags, owner,
            [&](DWORD forwarded)
            {
                Require(owner || locked, "GetData accessed the device outside the submission gate");
                Require(forwarded == flags, "Query flags changed, risking an unsubmitted fence or unwanted flush");
                ++polls;
                return status;
            }, [&]
            {
                ++acquires;
                return locked = canAcquire;
            }, [&]
            {
                Require(locked, "Released an unowned gate");
                locked = false;
                ++releases;
            });
        Require(result == ((owner || canAcquire) ? status : S_FALSE), "Wrong query status");
        Require(polls == unsigned(owner || canAcquire), "Query was skipped or executed twice");
        Require(acquires == unsigned(!owner) && releases == unsigned(!owner && canAcquire) && !locked,
            "Unbalanced query ownership");
    }

    void CheckConcurrentUpload()
    {
        using namespace std::chrono_literals;
        Components::Remix::Device::RenderFrameLock gate;
        std::promise<void> uploadStarted, finishUpload;
        bool chunkPresent = true;
        auto upload = std::async(std::launch::async, [&]
        {
            const std::lock_guard lock(gate);
            // Remix temporarily moves the current command chunk during submission.
            chunkPresent = false;
            uploadStarted.set_value();
            finishUpload.get_future().wait();
            chunkPresent = true;
        });
        uploadStarted.get_future().wait();
        unsigned polls = 0;
        const auto poll = [&](DWORD)
        {
            ++polls;
            Require(gate.OwnsCurrentThread() && chunkPresent, "Query observed a missing command chunk");
            return S_OK;
        };
        const auto pending = Components::Remix::Device::PollQuery(0, false, poll,
            [&] { return gate.TryLockFor(0ms); }, [&] { gate.unlock(); });
        finishUpload.set_value();
        upload.get();
        Require(pending == S_FALSE && polls == 0, "Busy upload did not defer the implicit query issue");
        const auto ready = Components::Remix::Device::PollQuery(D3DGETDATA_FLUSH, false, poll,
            [&] { return gate.TryLockFor(100ms); }, [&] { gate.unlock(); });
        Require(ready == S_OK && polls == 1, "Query did not resume after upload");
        const std::lock_guard frame(gate);
        bool reacquired = false;
        const auto nested = Components::Remix::Device::PollQuery(D3DGETDATA_FLUSH, gate.OwnsCurrentThread(), poll,
            [&] { reacquired = true; return false; }, [] {});
        Require(nested == S_OK && polls == 2 && !reacquired, "Render-thread flush no longer works");
    }
}

int main()
{
    try
    {
        for (bool owner : {false, true})
            for (bool acquire : {false, true})
                for (DWORD flags : {0UL, DWORD(D3DGETDATA_FLUSH), 0x80UL, DWORD(D3DGETDATA_FLUSH) | 0x80UL})
                    for (HRESULT status : {S_FALSE, S_OK, D3DERR_DEVICELOST, D3DERR_INVALIDCALL})
                        Check(flags, owner, acquire, status);
        CheckConcurrentUpload();
        std::cout << "Remix queries: all calls serialized, flags/errors preserved, busy uploads deferred and nested render flush passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
