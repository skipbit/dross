#include "ipc/last_error.h"
#include "ipc/poller.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

namespace dross {

namespace {

std::uint32_t events_of(const poller::interest wanted)
{
    const auto bits = static_cast<std::uint8_t>(wanted);
    std::uint32_t result = 0;
    if ((bits & static_cast<std::uint8_t>(poller::interest::read)) != 0) {
        result |= EPOLLIN;
    }
    if ((bits & static_cast<std::uint8_t>(poller::interest::write)) != 0) {
        result |= EPOLLOUT;
    }
    return result;
}

}  // namespace

// An epoll instance, and an eventfd registered with it that wake() writes to.
class poller::storage final {
public:
    storage(const int epoll, const int waker) noexcept
        : _epoll{ epoll }
        , _waker{ waker }
    {
    }

    ~storage()
    {
        ::close(_waker);
        ::close(_epoll);
    }

    storage(const storage& other) = delete;
    storage& operator=(const storage& other) = delete;

    std::expected<void, error> control(const int operation, const int fd, const std::uint32_t events)
    {
        epoll_event change{};
        change.events = events;
        change.data.fd = fd;
        if (::epoll_ctl(_epoll, operation, fd, &change) != 0) {
            return std::unexpected(last_error("epoll_ctl"));
        }
        return {};
    }

    void forget(const int fd) noexcept
    {
        epoll_event unused{};
        (void)::epoll_ctl(_epoll, EPOLL_CTL_DEL, fd, &unused);
    }

    std::expected<std::vector<event>, error> wait(const int timeout_ms)
    {
        std::array<epoll_event, 64> ready{};
        const int count = ::epoll_wait(_epoll, ready.data(), static_cast<int>(ready.size()), timeout_ms);
        if (count < 0) {
            if (errno == EINTR) {
                return std::vector<event>{};
            }
            return std::unexpected(last_error("epoll_wait"));
        }
        std::vector<event> result;
        for (int i = 0; i < count; ++i) {
            const epoll_event& each = ready[static_cast<std::size_t>(i)];
            if (each.data.fd == _waker) {
                std::uint64_t drained = 0;
                (void)! ::read(_waker, &drained, sizeof(drained));
                continue;
            }
            result.push_back(event{ each.data.fd, (each.events & EPOLLIN) != 0, (each.events & EPOLLOUT) != 0, (each.events & (EPOLLHUP | EPOLLERR)) != 0 });
        }
        return result;
    }

    void wake() noexcept
    {
        const std::uint64_t one = 1;
        (void)! ::write(_waker, &one, sizeof(one));
    }

private:
    const int _epoll;
    const int _waker;
};

std::expected<poller, error> poller::create()
{
    const int epoll = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll < 0) {
        return std::unexpected(last_error("epoll_create1"));
    }
    const int waker = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (waker < 0) {
        const error failure = last_error("eventfd");
        ::close(epoll);
        return std::unexpected(failure);
    }
    auto store = std::make_unique<storage>(epoll, waker);
    if (auto added = store->control(EPOLL_CTL_ADD, waker, EPOLLIN); ! added) {
        return std::unexpected(std::move(added).error());
    }
    return poller{ std::move(store) };
}

poller::poller(std::unique_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

poller::poller(poller&& other) noexcept = default;

poller& poller::operator=(poller&& other) noexcept = default;

poller::~poller() = default;

std::expected<void, error> poller::add(const int fd, const interest wanted)
{
    return _store->control(EPOLL_CTL_ADD, fd, events_of(wanted));
}

std::expected<void, error> poller::change(const int fd, const interest wanted)
{
    return _store->control(EPOLL_CTL_MOD, fd, events_of(wanted));
}

void poller::remove(const int fd) noexcept
{
    _store->forget(fd);
}

std::expected<std::vector<poller::event>, error> poller::wait(const std::optional<std::chrono::milliseconds> timeout)
{
    return _store->wait(timeout ? static_cast<int>(timeout->count()) : -1);
}

void poller::wake() noexcept
{
    _store->wake();
}

}  // namespace dross
