#include "advanced_model_download.h"

#include <bits.h>
#include <bcrypt.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
template <typename T>
void release(T *& value) {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

bool same_text(const wchar_t * left, const wchar_t * right) {
    return left != nullptr && right != nullptr && _wcsicmp(left, right) == 0;
}

IBackgroundCopyJob * find_existing_job(IBackgroundCopyManager * manager) {
    IEnumBackgroundCopyJobs * jobs = nullptr;
    if (FAILED(manager->EnumJobs(0, &jobs)) || jobs == nullptr) {
        return nullptr;
    }
    IBackgroundCopyJob * found = nullptr;
    while (true) {
        IBackgroundCopyJob * candidate = nullptr;
        ULONG fetched = 0;
        if (jobs->Next(1, &candidate, &fetched) != S_OK || fetched == 0) {
            break;
        }
        LPWSTR name = nullptr;
        if (SUCCEEDED(candidate->GetDisplayName(&name)) &&
            same_text(name, advanced_model::kDisplayName)) {
            CoTaskMemFree(name);
            found = candidate;
            break;
        }
        CoTaskMemFree(name);
        candidate->Release();
    }
    jobs->Release();
    return found;
}

std::string hex_digest(const std::array<unsigned char, 32> & digest) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2);
    for (const unsigned char value : digest) {
        result.push_back(digits[value >> 4]);
        result.push_back(digits[value & 0x0f]);
    }
    return result;
}

bool sha256_matches(const std::filesystem::path & path, const char * expected) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD object_size = 0;
    DWORD result_size = 0;
    std::vector<unsigned char> object;
    std::array<unsigned char, 32> digest{};
    bool okay = false;

    if (BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0 ||
        BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
            &result_size, 0) != 0) {
        goto cleanup;
    }
    object.resize(object_size);
    if (BCryptCreateHash(
            algorithm, &hash, object.data(), object_size,
            nullptr, 0, 0) != 0) {
        goto cleanup;
    }

    file = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) goto cleanup;
    {
        std::array<unsigned char, 1024 * 1024> buffer{};
        while (true) {
            DWORD count = 0;
            if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                          &count, nullptr)) {
                goto cleanup;
            }
            if (count == 0) break;
            if (BCryptHashData(hash, buffer.data(), count, 0) != 0) {
                goto cleanup;
            }
        }
    }
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) != 0) {
        goto cleanup;
    }
    okay = hex_digest(digest) == expected;

cleanup:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash != nullptr) BCryptDestroyHash(hash);
    if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
    return okay;
}
}

struct AdvancedModelDownloader::Impl {
    ~Impl() {
        stop.store(true, std::memory_order_release);
        if (thread.joinable()) thread.join();
    }

    void post(AdvancedModelState state, uint64_t transferred = 0,
              uint64_t total = advanced_model::kDownloadBytes,
              HRESULT error = S_OK) {
        auto * event = new AdvancedModelDownloadEvent{state, transferred, total, error};
        if (!PostMessageW(notify_window, notify_message, 0, reinterpret_cast<LPARAM>(event))) {
            delete event;
        }
    }

    void run(std::filesystem::path destination) {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool uninitialize = SUCCEEDED(initialized);
        IBackgroundCopyManager * manager = nullptr;
        IBackgroundCopyJob * job = nullptr;
        HRESULT failure = S_OK;
        BG_JOB_STATE initial_state{};

        std::error_code filesystem_error;
        std::filesystem::create_directories(destination.parent_path(), filesystem_error);
        const std::filesystem::path partial = destination.wstring() + L".download";

        failure = CoCreateInstance(
            __uuidof(BackgroundCopyManager), nullptr, CLSCTX_LOCAL_SERVER,
            __uuidof(IBackgroundCopyManager), reinterpret_cast<void **>(&manager));
        if (FAILED(failure) || manager == nullptr) goto failed;

        job = find_existing_job(manager);
        if (job == nullptr) {
            GUID identifier{};
            failure = manager->CreateJob(
                advanced_model::kDisplayName, BG_JOB_TYPE_DOWNLOAD, &identifier, &job);
            if (FAILED(failure) || job == nullptr) goto failed;
            std::filesystem::remove(partial, filesystem_error);
            failure = job->AddFile(advanced_model::kUrl, partial.c_str());
            if (FAILED(failure)) goto failed;
            job->SetDescription(
                L"Optional on-device Adapt writing model. Safe to pause or resume.");
            job->SetPriority(BG_JOB_PRIORITY_NORMAL);
        }
        failure = job->GetState(&initial_state);
        if (FAILED(failure)) goto failed;
        if (initial_state != BG_JOB_STATE_TRANSFERRED) {
            failure = job->Resume();
            if (FAILED(failure)) goto failed;
        }

        while (true) {
            if (cancel_requested.exchange(false, std::memory_order_acq_rel)) {
                job->Cancel();
                std::filesystem::remove(partial, filesystem_error);
                post(AdvancedModelState::Cancelled);
                goto done;
            }
            if (stop.load(std::memory_order_acquire)) goto done;

            BG_JOB_STATE state{};
            failure = job->GetState(&state);
            if (FAILED(failure)) goto failed;
            BG_JOB_PROGRESS progress{};
            if (SUCCEEDED(job->GetProgress(&progress))) {
                const uint64_t total = progress.BytesTotal == BG_SIZE_UNKNOWN
                    ? advanced_model::kDownloadBytes
                    : progress.BytesTotal;
                post(AdvancedModelState::Downloading, progress.BytesTransferred, total);
            }
            if (state == BG_JOB_STATE_TRANSFERRED) {
                failure = job->Complete();
                if (FAILED(failure)) goto failed;
                post(AdvancedModelState::Verifying, advanced_model::kDownloadBytes,
                     advanced_model::kDownloadBytes);
                if (std::filesystem::file_size(partial, filesystem_error) !=
                        advanced_model::kDownloadBytes || filesystem_error ||
                    !sha256_matches(partial, advanced_model::kSha256)) {
                    failure = TRUST_E_BAD_DIGEST;
                    std::filesystem::remove(partial, filesystem_error);
                    goto failed;
                }
                std::filesystem::remove(destination, filesystem_error);
                filesystem_error.clear();
                std::filesystem::rename(partial, destination, filesystem_error);
                if (filesystem_error) {
                    failure = HRESULT_FROM_WIN32(filesystem_error.value());
                    goto failed;
                }
                post(AdvancedModelState::Installed, advanced_model::kDownloadBytes,
                     advanced_model::kDownloadBytes);
                goto done;
            }
            if (state == BG_JOB_STATE_ERROR || state == BG_JOB_STATE_TRANSIENT_ERROR) {
                if (state == BG_JOB_STATE_ERROR) {
                    IBackgroundCopyError * error = nullptr;
                    if (SUCCEEDED(job->GetError(&error)) && error != nullptr) {
                        BG_ERROR_CONTEXT context{};
                        error->GetError(&context, &failure);
                        error->Release();
                    } else {
                        failure = E_FAIL;
                    }
                    goto failed;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
        }
        goto done;

failed:
        if (job != nullptr) job->Cancel();
        post(AdvancedModelState::Failed, 0, advanced_model::kDownloadBytes, failure);
done:
        release(job);
        release(manager);
        if (uninitialize) CoUninitialize();
        active.store(false, std::memory_order_release);
    }

    HWND notify_window = nullptr;
    UINT notify_message = 0;
    std::thread thread;
    std::atomic<bool> active{false};
    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> stop{false};
};

AdvancedModelDownloader::AdvancedModelDownloader() : impl_(std::make_unique<Impl>()) {}
AdvancedModelDownloader::~AdvancedModelDownloader() = default;

bool AdvancedModelDownloader::start(
    HWND notify_window, UINT notify_message, std::filesystem::path destination) {
    if (impl_->active.load(std::memory_order_acquire)) return false;
    if (impl_->thread.joinable()) impl_->thread.join();
    impl_->notify_window = notify_window;
    impl_->notify_message = notify_message;
    impl_->cancel_requested.store(false, std::memory_order_release);
    impl_->stop.store(false, std::memory_order_release);
    impl_->active.store(true, std::memory_order_release);
    impl_->thread = std::thread(&Impl::run, impl_.get(), std::move(destination));
    return true;
}

void AdvancedModelDownloader::cancel() {
    impl_->cancel_requested.store(true, std::memory_order_release);
}

bool AdvancedModelDownloader::active() const {
    return impl_->active.load(std::memory_order_acquire);
}
