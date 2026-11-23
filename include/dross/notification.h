/**
 * @file notification.h
 * @brief Notification module: sending typed notifications from a
 * broadcaster to whoever subscribes through a listener, each on the thread
 * the subscriber chose, within a process or from one process to another.
 *
 * Usage:
 * @code
 * #include <dross/notification.h>
 *
 * struct progress {
 *     int done;
 *     int total;
 * };
 *
 * inline constexpr dross::topic<progress> kProgress{ "com.example.player.progress" };
 *
 * void show(const progress& now);
 *
 * int main()
 * {
 *     auto news = dross::broadcaster::create();
 *     auto ears = dross::listener::create(news);
 *
 *     auto subscribed = ears.subscribe(kProgress, show, dross::delivery::queued());
 *     if (! subscribed) {
 *         return 1;
 *     }
 *     dross::subscription keep = std::move(*subscribed);
 *
 *     news.notify(kProgress, progress{ 3, 10 });
 *     dross::runloop::current().run_pending();  // show() runs here
 * }
 * @endcode
 *
 * The pieces:
 * - dross::topic names what is sent and the type of value it carries
 * - dross::broadcaster sends, and does nothing else
 * - dross::listener subscribes to one broadcaster's notifications, and does
 *   not send
 * - dross::notification is what one call to notify() sent, and cancels the
 *   deliveries of it that have not started
 * - dross::notify_options says how notify() sends; notify_timing::deferred
 *   holds back the subscribers that run on the sending thread until notify()
 *   has returned
 * - dross::observable holds a value that its owner thread changes, and
 *   calls the functions given to when_changed() on each change, where each
 *   one's delivery says
 * - dross::link_event is what dross::listener::link reports about a
 *   listener's connection to another process
 *
 * A subscriber names, for every subscription, where its function runs:
 *
 * - dross::delivery::queued() on a run loop, the subscribing thread's by
 *   default
 * - dross::delivery::direct() on the sending thread, inside the call that
 *   sends, or on that thread's run loop once it has returned when the sender
 *   defers it
 * - dross::delivery::on() on one of an operation_queue's workers
 *
 * There is no default, so reading a subscription tells which thread its
 * function runs on.
 *
 * Subscribing gives back a dross::subscription. Holding it keeps the
 * function being called; destroying it or calling reset() stops the
 * notifications still queued from calling it, without waiting for a call on
 * another thread.
 *
 * There is no broadcaster shared by the whole process: each one is made by
 * create() and handed to whoever sends or listens.
 *
 * Between processes:
 * - broadcaster::create() given a place, such as a dross::ipc::unix_socket,
 *   publishes the broadcaster there, and listener::create() given the same
 *   place hears it from another process. The listener can be made first,
 *   and connects again whenever the connection ends
 * - A value crosses as a dross::value, so a topic sent or subscribed to
 *   between processes carries a type with dross::value_traits. Between
 *   processes a topic is told apart by its name alone
 * - A notification reaches a listener in another process only once its
 *   subscription has reached the broadcaster
 * - What happens to the connection is reported on dross::listener::link
 *
 * See dross::broadcaster and dross::listener for the details.
 *
 * Errors:
 * - listener::subscribe() returns an error of notification_errc, rather than
 *   a subscription, for an empty function, and on a listener made with a
 *   place, for a type without dross::value_traits. observable::when_changed()
 *   returns a subscription that is not active for an empty function
 * - broadcaster::create() with a place returns the error that kept it from
 *   listening there
 * - notify() on a published broadcaster with a type without
 *   dross::value_traits is a misuse, and stops the program
 * - A subscription whose run loop's thread has ended, whose queue has
 *   stopped, or whose broadcaster or observable has gone is no longer active
 *   rather than failing; active() reports it
 * - observable::get() from a thread that does not own the observable, or on
 *   a moved-from one, is a misuse, and stops the program
 * - An exception thrown by a subscriber's function propagates out of the
 *   call that ran it. It is not caught, stored or translated
 *
 * Platform support:
 * - Wherever the thread module runs, with run-time type information on
 * - Between processes over a dross::ipc::unix_socket: Linux and macOS,
 *   between processes of the same effective user
 */

#pragma once

#include <dross/notification/broadcaster.h>
#include <dross/notification/delivery.h>
#include <dross/notification/error.h>
#include <dross/notification/link_event.h>
#include <dross/notification/listener.h>
#include <dross/notification/notification.h>
#include <dross/notification/observable.h>
#include <dross/notification/subscription.h>
#include <dross/notification/topic.h>

/**
 * @brief Notification namespace members live directly in dross.
 *
 * The notification module adds topic, broadcaster, listener, notification,
 * notify_options, observable, delivery and subscription to the dross
 * namespace, alongside the thread module it runs on, rather than a
 * namespace of its own.
 */
