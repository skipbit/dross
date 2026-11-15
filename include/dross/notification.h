/**
 * @file notification.h
 * @brief Notification module: choosing where a subscriber is called, and
 * holding the right to be called.
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
 * function being called; destroying it or calling reset() stops new calls
 * from starting, without waiting for one already running.
 *
 * Errors:
 * - A subscription whose run loop's thread has ended, or whose queue has
 *   stopped, is no longer active rather than failing; active() reports it
 * - An exception thrown by a subscriber's function propagates out of the
 *   call that ran it. It is not caught, stored or translated
 *
 * Platform support:
 * - Wherever the thread module runs
 */

#pragma once

#include <dross/notification/delivery.h>
#include <dross/notification/subscription.h>

/**
 * @brief Notification namespace members live directly in dross.
 *
 * The notification module adds delivery and subscription to the dross
 * namespace, alongside the thread module it runs on, rather than a namespace
 * of its own.
 */
