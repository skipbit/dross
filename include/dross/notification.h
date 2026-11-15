/**
 * @file notification.h
 * @brief Notification module: sending typed notifications from a
 * broadcaster to whoever subscribes through a listener, each on the thread
 * the subscriber chose.
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
 *     dross::current_runloop().run_pending();  // show() runs here
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
 *
 * A subscriber names, for every subscription, where its function runs:
 *
 * - dross::delivery::queued() on a run loop, the subscribing thread's by
 *   default
 * - dross::delivery::direct() on the sending thread, inside the call that
 *   sends
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
 * Errors:
 * - listener::subscribe() returns an error of notification_errc, rather than
 *   a subscription, for an empty function
 * - A subscription whose run loop's thread has ended, whose queue has
 *   stopped, or whose broadcaster has gone is no longer active rather than
 *   failing; active() reports it
 * - An exception thrown by a subscriber's function propagates out of the
 *   call that ran it. It is not caught, stored or translated
 *
 * Platform support:
 * - Wherever the thread module runs, with run-time type information on
 */

#pragma once

#include <dross/notification/broadcaster.h>
#include <dross/notification/delivery.h>
#include <dross/notification/error.h>
#include <dross/notification/listener.h>
#include <dross/notification/notification.h>
#include <dross/notification/subscription.h>
#include <dross/notification/topic.h>

/**
 * @brief Notification namespace members live directly in dross.
 *
 * The notification module adds topic, broadcaster, listener, notification,
 * notify_options, delivery and subscription to the dross namespace,
 * alongside the thread module it runs on, rather than a namespace of its
 * own.
 */
