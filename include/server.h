#ifndef SERVER_H
#define SERVER_H

#include <map>
#include <vector>
#include <memory>
#include <pthread.h>
#include "util.h"
#include "model.h"
#include "message_queue.h"

// Forward declarations
class Client;
class Message;

class Server {
private:
  std::map<int, std::shared_ptr<Order>> m_orders;  //active orders
  int m_next_order_id;                              //next order id, starts 1000
  pthread_mutex_t m_lock;                           //protects orders + display clients
  std::vector<MessageQueue *> m_display_clients;    //active display client queues

  // no value semantics
  NO_VALUE_SEMANTICS(Server);

public:
  Server();
  ~Server();

  // Create a server socket listening on specified port,
  // and accept connections from clients. This function does
  // not return.
  void server_loop(const char *port);

  //add new order, assign id, return assigned id
  int add_order(std::shared_ptr<Order> order);

  //update item status, adds msgs to broadcast vec
  void update_item(int order_id, int item_id, ItemStatus new_status,
                   std::vector<std::shared_ptr<Message>> &broadcasts);

  //update order status (DONE->DELIVERED only)
  void update_order(int order_id, OrderStatus new_status,
                    std::vector<std::shared_ptr<Message>> &broadcasts);

  //get all current orders (for new display client)
  std::vector<std::shared_ptr<Order>> get_all_orders();

  //register display client queue
  void add_display_client(MessageQueue *mq);

  //unregister display client queue
  void remove_display_client(MessageQueue *mq);

  //broadcast msg to all display clients
  void broadcast(std::shared_ptr<Message> msg);

private:
  // no private helpers needed
};

#endif // SERVER_H
