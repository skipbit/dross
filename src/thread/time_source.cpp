#include "thread/time_source.h"

namespace dross {

namespace {

class steady_source final : public time_source {
public:
    time_point now() const override
    {
        return std::chrono::steady_clock::now();
    }

    void wait(std::unique_lock<std::mutex>& lock, std::condition_variable& wake) const override
    {
        wake.wait(lock);
    }

    void wait_until(std::unique_lock<std::mutex>& lock, std::condition_variable& wake, time_point deadline) const override
    {
        wake.wait_until(lock, deadline);
    }
};

}  // namespace

std::shared_ptr<const time_source> time_source::steady()
{
    // Never destroyed, for the same reason as the main loop: a thread still
    // running when the program ends may still be waiting on it.
    static const std::shared_ptr<const time_source>* const the_source = new std::shared_ptr<const time_source>{ std::make_shared<steady_source>() };
    return *the_source;
}

}  // namespace dross
