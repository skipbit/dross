#include "notification/poller.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <iterator>
#include <string>
#include <sys/event.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace dross {

namespace {

// The identifier of the user event wake() triggers. User events have their
// own identifiers, apart from file descriptors.
constexpr std::uintptr_t kWakeIdent = 0;

error last_error(const char* call)
{
    return error{ static_cast<std::errc>(errno), call };
}

bool wants(const poller::interest wanted, const poller::interest one)
{
    return (static_cast<std::uint8_t>(wanted) & static_cast<std::uint8_t>(one)) != 0;
}

}  // namespace

// A kqueue, with a user event that wake() triggers.
class poller::storage final {
public:
    explicit storage(const int queue) noexcept
        : _queue{ queue }
    {
    }

    ~storage()
    {
        ::close(_queue);
    }

    storage(const storage& other) = delete;
    storage& operator=(const storage& other) = delete;

    std::expected<void, error> apply(const std::uintptr_t ident, const short filter, const unsigned short flags, const unsigned int fflags)
    {
        struct kevent change;
        EV_SET(&change, ident, filter, flags, fflags, 0, nullptr);
        if (::kevent(_queue, &change, 1, nullptr, 0, nullptr) != 0) {
            return std::unexpected(last_error("kevent"));
        }
        return {};
    }

    // Removes a filter that may not be there.
    void drop(const int fd, const short filter) noexcept
    {
        struct kevent change;
        EV_SET(&change, static_cast<std::uintptr_t>(fd), filter, EV_DELETE, 0, 0, nullptr);
        (void)::kevent(_queue, &change, 1, nullptr, 0, nullptr);
    }

    std::expected<void, error> set(const int fd, const interest wanted)
    {
        const auto ident = static_cast<std::uintptr_t>(fd);
        if (wants(wanted, interest::read)) {
            if (auto added = apply(ident, EVFILT_READ, EV_ADD, 0); ! added) {
                return added;
            }
        } else {
            drop(fd, EVFILT_READ);
        }
        if (wants(wanted, interest::write)) {
            if (auto added = apply(ident, EVFILT_WRITE, EV_ADD, 0); ! added) {
                return added;
            }
        } else {
            drop(fd, EVFILT_WRITE);
        }
        return {};
    }

    std::expected<std::vector<event>, error> wait(const std::optional<std::chrono::milliseconds> timeout)
    {
        std::array<struct kevent, 64> ready{};
        timespec limit{};
        if (timeout) {
            limit.tv_sec = static_cast<std::time_t>(timeout->count() / 1000);
            limit.tv_nsec = static_cast<long>((timeout->count() % 1000) * 1000000);
        }
        const int count = ::kevent(_queue, nullptr, 0, ready.data(), static_cast<int>(ready.size()), timeout ? &limit : nullptr);
        if (count < 0) {
            if (errno == EINTR) {
                return std::vector<event>{};
            }
            return std::unexpected(last_error("kevent"));
        }
        // kqueue reports reading and writing as separate events; they are
        // joined into one event for each descriptor.
        std::vector<event> result;
        for (int i = 0; i < count; ++i) {
            const struct kevent& each = ready[static_cast<std::size_t>(i)];
            if (each.filter == EVFILT_USER) {
                continue;
            }
            const int fd = static_cast<int>(each.ident);
            auto found = result.begin();
            while ((found != result.end()) && (found->fd != fd)) {
                ++found;
            }
            if (found == result.end()) {
                result.push_back(event{ fd, false, false, false });
                found = std::prev(result.end());
            }
            found->readable = found->readable || (each.filter == EVFILT_READ);
            found->writable = found->writable || (each.filter == EVFILT_WRITE);
            found->hangup = found->hangup || ((each.flags & (EV_EOF | EV_ERROR)) != 0);
        }
        return result;
    }

    void wake() noexcept
    {
        struct kevent trigger;
        EV_SET(&trigger, kWakeIdent, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
        (void)::kevent(_queue, &trigger, 1, nullptr, 0, nullptr);
    }

private:
    const int _queue;
};

std::expected<poller, error> poller::create()
{
    const int queue = ::kqueue();
    if (queue < 0) {
        return std::unexpected(last_error("kqueue"));
    }
    auto store = std::make_unique<storage>(queue);
    if (::fcntl(queue, F_SETFD, FD_CLOEXEC) != 0) {
        return std::unexpected(last_error("fcntl"));
    }
    if (auto added = store->apply(kWakeIdent, EVFILT_USER, EV_ADD | EV_CLEAR, 0); ! added) {
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
    return _store->set(fd, wanted);
}

std::expected<void, error> poller::change(const int fd, const interest wanted)
{
    return _store->set(fd, wanted);
}

void poller::remove(const int fd) noexcept
{
    _store->drop(fd, EVFILT_READ);
    _store->drop(fd, EVFILT_WRITE);
}

std::expected<std::vector<poller::event>, error> poller::wait(const std::optional<std::chrono::milliseconds> timeout)
{
    return _store->wait(timeout);
}

void poller::wake() noexcept
{
    _store->wake();
}

}  // namespace dross
