#pragma once

#include <memory>

namespace dross {

/**
 * @brief One notification, as broadcaster::notify() sent it.
 *
 * notify() returns one for every call. It names that one sending, not the
 * kind of notification, which is what a dross::topic names. Keeping it is
 * optional; most callers drop it.
 *
 * Handle semantics:
 * - A notification is a handle. Copying gives another handle to the same
 *   sending
 *
 * It offers nothing to do with the sending yet.
 */
class notification final {
public:
    /**
     * @brief Copy constructor, giving another handle to the same sending.
     * @param other The handle to copy
     */
    notification(const notification& other);

    /**
     * @brief Destructor.
     */
    ~notification();

    /**
     * @brief Copy assignment, naming the same sending as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    notification& operator=(const notification& other);

private:
    class storage;

    explicit notification(std::shared_ptr<storage> store) noexcept;

    std::shared_ptr<storage> _store;

    friend class broadcaster;
};

}  // namespace dross
