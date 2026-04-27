README for Assignment 5

Names of team members, contributions:

Edmund: wire.cpp, io.cpp, message model execpt, server, concurrency report

Evan: client util, updater main, display main, server

## Concurrency Report

The server uses a multi-threaded architecture, handling incoming connections by launching a detached client thread
for each one. The system relies on several pieces of shared states across the threads. This shared data includes the
server’s currently active orders, the counter for the next order ID, the roster of active display clients, and the 
individual pending message queues assigned to each of those display clients.

To protect its core shared state the system uses the Server::m_lock mutex. A Guard object is used to ensure this mutex
is consistently held and released whenever the shared data is read or modified. Additionally, every MessageQueue also 
has a dedicated mutex and semaphore. The mutex protects the internal queue structure, while the semaphore tracks the number
of queued messages. The semaphore allows the dequeue() operation to wait for up to one second before timing out and
returning a null pointer, which acts as a heartbeat mechanism.

The server also takes specific precautions to avoid deadlocks when distributing broadcast messages. It avoids
holding the Server::m_lock while pushing messages into the individual client queues. Instead, the server briefly acquires
the lock to take a snapshot of the active display client queues. Once the snapshot is captured, the server lock 
is released, and the system iterates through the snapshot to enqueue a copy of the broadcast message for each client.

Finally, the cleanup of client resources is done with RAII. Every client thread holds ownership of its underlying Client
object via a std::unique_ptr. When a thread terminates and this pointer goes out of scope, the Client destructor is
automatically called. This destructor handles the cleanup by safely unregistering the display client from the server
and closing the associated network socket.