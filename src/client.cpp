#include <iostream>
#include <cassert>
#include <unistd.h>
#include "message.h"
#include "wire.h"
#include "io.h"
#include "except.h"
#include "server.h"
#include "passwd_db.h"
#include "client.h"

namespace {

//helper: encode msg and send over fd
void send_msg(const Message &msg, int fd) {
  std::string encoded;
  Wire::encode(msg, encoded);
  IO::send(encoded, fd);
}

//helper: receive and decode msg from fd
Message recv_msg(int fd) {
  std::string s;
  IO::receive(fd, s);
  Message msg;
  Wire::decode(s, msg);
  return msg;
}

} // anonymous namespace

Client::Client(int fd, Server *server)
  : m_fd(fd)
  , m_server(server)
  , m_mode(ClientMode::INVALID) {
}

Client::~Client() {
  if (m_mode == ClientMode::DISPLAY) {
    m_server->remove_display_client(&m_msg_queue);
  }
  close(m_fd);
}

void Client::chat() {
  //receive LOGIN
  Message login = recv_msg(m_fd);

  if (login.get_type() != MessageType::LOGIN)
    throw ProtocolError("Expected LOGIN message");

  //check credentials
  if (!PasswordDB::authenticate(login.get_str())) {
    send_msg(Message(MessageType::ERROR, "Invalid credentials"), m_fd);
    return;
  }

  //store client mode
  m_mode = login.get_client_mode();

  send_msg(Message(MessageType::OK, "Welcome"), m_fd);

  if (m_mode == ClientMode::UPDATER) {
    updater_loop();

  } else if (m_mode == ClientMode::DISPLAY) {
    //register before sending current orders
    m_server->add_display_client(&m_msg_queue);

    //send all current orders
    std::vector<std::shared_ptr<Order>> orders = m_server->get_all_orders();
    for (auto &order : orders) {
      send_msg(Message(MessageType::DISP_ORDER, order), m_fd);
    }

    display_loop();

  } else {
    throw ProtocolError("Unknown client mode");
  }
}

void Client::updater_loop() {
  while (true) {
    Message msg = recv_msg(m_fd);

    if (msg.get_type() == MessageType::QUIT) {
      send_msg(Message(MessageType::OK, "Goodbye"), m_fd);
      return;

    } else if (msg.get_type() == MessageType::ORDER_NEW) {
      std::shared_ptr<Order> order = msg.get_order();
      int assigned_id = m_server->add_order(order);
      send_msg(Message(MessageType::OK, "Created order id " + std::to_string(assigned_id)), m_fd);

    } else if (msg.get_type() == MessageType::ITEM_UPDATE) {
      try {
        std::vector<std::shared_ptr<Message>> broadcasts;
        m_server->update_item(msg.get_order_id(), msg.get_item_id(),
                              msg.get_item_status(), broadcasts);
        send_msg(Message(MessageType::OK, "Item updated"), m_fd);
      } catch (SemanticError &e) {
        send_msg(Message(MessageType::ERROR, e.what()), m_fd);
      }

    } else if (msg.get_type() == MessageType::ORDER_UPDATE) {
      try {
        std::vector<std::shared_ptr<Message>> broadcasts;
        m_server->update_order(msg.get_order_id(), msg.get_order_status(), broadcasts);
        send_msg(Message(MessageType::OK, "Order updated"), m_fd);
      } catch (SemanticError &e) {
        send_msg(Message(MessageType::ERROR, e.what()), m_fd);
      }

    } else {
      throw ProtocolError("Unexpected message type in updater loop");
    }
  }
}

void Client::display_loop() {
  while (true) {
    std::shared_ptr<Message> msg = m_msg_queue.dequeue();

    if (msg) {
      //got a real message, send it
      send_msg(*msg, m_fd);
    } else {
      //timeout, send heartbeat
      send_msg(Message(MessageType::DISP_HEARTBEAT), m_fd);
    }
  }
}
