README for Assignment 5

Names of team members, contributions:

Edmund: wire.cpp, io.cpp, message model execpt

Evan: client util, updater main, display main

## Concurrency / Synchronization Report

The server is multi-threaded: it accepts connections and starts one detached client thread per connection. The shared data are the server’s active orders, next order id, and active display client list, plus each display client’s pending message queue.

`Server::m_lock` protects all server-owned shared state: `m_orders`, `m_next_order_id`, and `m_display_clients`. The code uses `Guard` to hold and release this mutex consistently in the server methods that read or update shared state.

Each `MessageQueue` has its own mutex and semaphore. The mutex protects the internal queue, and the semaphore counts queued messages so `dequeue()` can wait up to one second for a message before returning a null pointer for a heartbeat.

To avoid deadlocks, the server does not hold `Server::m_lock` while enqueueing broadcast messages. It snapshots the active display client queues under the server lock, releases that lock, and then enqueues a copied message to each queue. Client cleanup is handled with RAII: each client thread owns a `Client` object through `std::unique_ptr`, and the client destructor removes display clients from the server and closes the socket.
