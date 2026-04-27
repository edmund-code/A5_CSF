#include <iostream>
#include <memory>
#include <cstring>
#include <cerrno>
#include "csapp.h"
#include "except.h"
#include "model.h"
#include "message.h"
#include "wire.h"
#include "client.h"
#include "guard.h"
#include "server.h"

namespace {

void *client_thread_fn(void *arg) {
  pthread_detach(pthread_self());
  std::unique_ptr<Client> client(static_cast<Client *>(arg));
  try {
    client->chat();
  } catch (...) {
    //client disconnected or error, destructor cleans up
  }
  return nullptr;
}

} // anonymous namespace

Server::Server()
  : m_next_order_id(1000) {
  pthread_mutex_init(&m_lock, nullptr);
}

Server::~Server() {
  pthread_mutex_destroy(&m_lock);
}

void Server::server_loop(const char *port) {
  int server_fd = open_listenfd(port);
  if (server_fd < 0)
    throw IOException(std::string("open_listenfd failed: ") + std::strerror(errno));

  while (true) {
    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      continue;
    }

    Client *client = new Client(client_fd, this);
    pthread_t tid;
    Pthread_create(&tid, nullptr, client_thread_fn, client);
  }
}

int Server::add_order(std::shared_ptr<Order> order) {
  std::shared_ptr<Message> bcast_msg;

  {
    Guard g(m_lock);

    int assigned_id = m_next_order_id++;
    order->set_id(assigned_id);
    for (int i = 0; i < order->get_num_items(); i++) {
      order->at(i)->set_order_id(assigned_id);
    }
    m_orders[assigned_id] = order;

    bcast_msg = std::make_shared<Message>(MessageType::DISP_ORDER, order->duplicate());
  }

  //broadcast outside lock to avoid nesting server lock + queue lock
  broadcast(bcast_msg);

  return order->get_id();
}

void Server::update_item(int order_id, int item_id, ItemStatus new_status,
                         std::vector<std::shared_ptr<Message>> &broadcasts) {
  {
    Guard g(m_lock);

    auto it = m_orders.find(order_id);
    if (it == m_orders.end())
      throw SemanticError("Order not found: " + std::to_string(order_id));

    std::shared_ptr<Order> order = it->second;
    std::shared_ptr<Item> item = order->find_item(item_id);
    if (!item)
      throw SemanticError("Item not found: " + std::to_string(item_id));

    //validate forward-only transition
    ItemStatus cur = item->get_status();
    if (cur == ItemStatus::NEW && new_status != ItemStatus::IN_PROGRESS)
      throw SemanticError("Invalid item status transition");
    if (cur == ItemStatus::IN_PROGRESS && new_status != ItemStatus::DONE)
      throw SemanticError("Invalid item status transition");
    if (cur == ItemStatus::DONE)
      throw SemanticError("Item already done");

    item->set_status(new_status);

    broadcasts.push_back(std::make_shared<Message>(
      MessageType::DISP_ITEM_UPDATE, order_id, item_id, new_status));

    //auto-transition order status
    OrderStatus order_cur = order->get_status();

    if (new_status == ItemStatus::IN_PROGRESS && order_cur == OrderStatus::NEW) {
      order->set_status(OrderStatus::IN_PROGRESS);
      broadcasts.push_back(std::make_shared<Message>(
        MessageType::DISP_ORDER_UPDATE, order_id, OrderStatus::IN_PROGRESS));

    } else if (new_status == ItemStatus::DONE) {
      bool all_done = true;
      for (int i = 0; i < order->get_num_items(); i++) {
        if (order->at(i)->get_status() != ItemStatus::DONE) {
          all_done = false;
          break;
        }
      }
      if (all_done && order_cur == OrderStatus::IN_PROGRESS) {
        order->set_status(OrderStatus::DONE);
        broadcasts.push_back(std::make_shared<Message>(
          MessageType::DISP_ORDER_UPDATE, order_id, OrderStatus::DONE));
      }
    }
  }

  //broadcast outside lock
  for (auto &msg : broadcasts) {
    broadcast(msg);
  }
}

void Server::update_order(int order_id, OrderStatus new_status,
                          std::vector<std::shared_ptr<Message>> &broadcasts) {
  std::shared_ptr<Message> bcast_msg;

  {
    Guard g(m_lock);

    auto it = m_orders.find(order_id);
    if (it == m_orders.end())
      throw SemanticError("Order not found: " + std::to_string(order_id));

    std::shared_ptr<Order> order = it->second;

    //only DONE->DELIVERED allowed
    if (order->get_status() != OrderStatus::DONE || new_status != OrderStatus::DELIVERED)
      throw SemanticError("Invalid order status transition");

    order->set_status(OrderStatus::DELIVERED);
    bcast_msg = std::make_shared<Message>(
      MessageType::DISP_ORDER_UPDATE, order_id, OrderStatus::DELIVERED);
    broadcasts.push_back(bcast_msg);

    m_orders.erase(it);
  }

  broadcast(bcast_msg);
}

std::vector<std::shared_ptr<Order>> Server::get_all_orders() {
  Guard g(m_lock);
  std::vector<std::shared_ptr<Order>> result;
  for (auto &kv : m_orders) {
    result.push_back(kv.second->duplicate());
  }
  return result;
}

void Server::add_display_client(MessageQueue *mq) {
  Guard g(m_lock);
  m_display_clients.push_back(mq);
}

void Server::remove_display_client(MessageQueue *mq) {
  Guard g(m_lock);
  for (auto it = m_display_clients.begin(); it != m_display_clients.end(); ++it) {
    if (*it == mq) {
      m_display_clients.erase(it);
      break;
    }
  }
}

void Server::broadcast(std::shared_ptr<Message> msg) {
  //snapshot display clients under lock, then enqueue outside lock
  std::vector<MessageQueue *> snapshot;
  {
    Guard g(m_lock);
    snapshot = m_display_clients;
  }
  for (MessageQueue *mq : snapshot) {
    mq->enqueue(msg->duplicate());
  }
}
