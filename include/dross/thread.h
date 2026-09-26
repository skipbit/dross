/**
 * @file thread.h
 * @brief Thread module: running a loop on a thread and handing it work.
 *
 * The standard library starts a thread and waits for it to finish. This
 * module covers what happens in between: a thread keeps a loop, other
 * threads hand that loop a task, and the loop runs it. Two things follow
 * from that and are the point of the module:
 *
 * - work can be aimed at a particular thread, rather than at whichever
 *   worker happens to be free
 * - a callback can be sent back to the thread the work came from
 *
 * operation_queue covers the other case, work for whichever of a fixed set
 * of workers is free. Its workers keep a loop too, so the way back works
 * from them as well. Where the caller would rather wait for what the work
 * returns than have it sent back, enqueue() hands back an operation_result
 * to wait on and read.
 *
 * Usage:
 * @code
 * #include <dross/thread.h>
 * using namespace dross;
 *
 * // The main thread's loop, taken on the main thread and handed to a worker.
 * runloop main_loop = main_runloop();
 *
 * std::thread worker{[main_loop]() mutable {
 *     auto answer = expensive_work();
 *     // Back on the main thread, not on this one.
 *     main_loop.perform([answer]() { show(answer); });
 * }};
 *
 * main_runloop().run();  // until something calls quit()
 * worker.join();
 * @endcode
 *
 * Identity and lifetime:
 * - A runloop is a handle. Copying one gives another handle to the same
 *   loop, and operator== asks whether two handles name the same loop
 * - One thread has one loop. It is reached through current_runloop(), or
 *   through main_runloop() for the main thread's, and is never constructed
 *   directly
 * - dross::thread starts and names a thread, with the same handle
 *   semantics; a thread dross did not start, such as the process's own, is
 *   named through main_thread() and current_thread()
 * - dross::timer installs a callback on a loop, firing it once or on an
 *   interval; it is the same kind of handle again, but the loop is what
 *   keeps it alive once installed, not the handle
 * - dross::operation_queue is the same kind of handle; the last one going
 *   stops the queue accepting tasks, and its workers end once they have run
 *   what is left
 *
 * Errors:
 * - perform() returns false when the task cannot be queued, which the
 *   caller can act on
 * - A timer factory call that cannot install, because callback is empty or
 *   loop has already finished, returns a handle that is already invalid
 *   rather than failing outright
 * - operation_result::get_as() returns an error of operation_errc, rather
 *   than a value, for a result that is not finished, was cancelled, or is
 *   not of the type asked for
 * - An exception thrown by a task or a timer's callback propagates out of
 *   the run() call that was running it. It is not caught, stored or
 *   translated
 *
 * Platform support:
 * - Linux and macOS; native.cpp does not compile for anything else
 */

#pragma once

#include <dross/thread/operation.h>
#include <dross/thread/operation_queue.h>
#include <dross/thread/runloop.h>
#include <dross/thread/thread.h>
#include <dross/thread/timer.h>

/**
 * @brief Threading namespace members live directly in dross.
 *
 * The thread module adds runloop, thread, timer, operation_queue and
 * operation_result to the dross namespace, alongside the type and platform
 * layers, rather than a namespace of its own.
 */
