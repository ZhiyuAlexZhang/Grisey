#pragma once

#include <atomic>

//==============================================================================
// Raises an atomic to a value if the value is higher, without a lock. The audio
// thread uses it to publish peaks, which the GUI thread reads and resets with
// exchange(), so a peak is never lost between the two.
inline void storeMax(std::atomic<float>& target, float value)
{
    float current = target.load(std::memory_order_relaxed);
    while (value > current && ! target.compare_exchange_weak(current, value, std::memory_order_relaxed))
    {
    }
}
