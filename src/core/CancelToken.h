#pragma once

#include <atomic>
#include <memory>

class CancelToken
{
public:
    CancelToken() = default;

    CancelToken(const CancelToken &) = delete;
    CancelToken &operator=(const CancelToken &) = delete;

    void cancel() { m_cancelled.store(true, std::memory_order_relaxed); }
    void reset() { m_cancelled.store(false, std::memory_order_relaxed); }

    [[nodiscard]] bool isCancelled() const
    {
        return m_cancelled.load(std::memory_order_relaxed);
    }

private:
    std::atomic<bool> m_cancelled{false};
};

using CancelTokenPtr = std::shared_ptr<CancelToken>;
