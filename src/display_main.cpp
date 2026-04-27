/*
 * main program for the display client
 */

#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <stdexcept>
#include "csapp.h"
#include "model.h"
#include "message.h"
#include "wire.h"
#include "io.h"
#include "except.h"
#include "util.h"
#include "client_util.h"

namespace {

// this terminal escape sequence should be written to
// std::cout before the display client redisplays the current
// order info
const char CLEAR_SCREEN[] = "\x1b[2J\x1b[H";

// print all curr orders to stdout, clear screen first
void print_all_orders(const std::map<int, std::shared_ptr<Order>> &orders) {

  std::cout << CLEAR_SCREEN;

  for (auto &pair : orders) {
    auto order = pair.second;

    std::cout << "Order " << order->get_id() << ": "
              << Wire::order_status_to_str(order->get_status()) << "\n";

    for (int i = 0; i < order->get_num_items(); i++) {
      auto item = order->at(i);

      std::cout << "  Item " << item->get_id() << ": "

                << Wire::item_status_to_str(item->get_status()) << "\n";
      std::cout << "    " << item->get_desc() << ", Quantity " << item->get_qty() << "\n";
    }
  }

  std::cout.flush();

}

}

int main(int argc, char **argv) {
  
  if (argc != 3) {
    
    std::cerr << "Usage: " << argv[0] << " <hostname> <port>\n";
    return 1;
  }

  int fd = open_clientfd(argv[1], argv[2]);
  
  if (fd < 0) {
    
    std::cerr << "Error: could not connect to server\n";

    return 1;
  }

  try {

    // get login credentials from user
    std::string username, password;
    std::cout << "username: ";

    std::getline(std::cin, username);
    std::cout << "password: ";

    std::getline(std::cin, password);

    std::string creds = username + "/" + password;

    // send login as display client
    send_msg(Message(MessageType::LOGIN, ClientMode::DISPLAY, creds), fd);

    // check server response
    Message resp = recv_msg(fd);
    if (resp.get_type() == MessageType::ERROR) {

      std::cerr << "Error: " << resp.get_str() << "\n";
      close(fd);

      return 1;
    } else if (resp.get_type() != MessageType::OK) {
      std::cerr << "Error: unexpected login response\n";
      close(fd);
      return 1;
    }

    // clear screen on startup
    std::cout << CLEAR_SCREEN;
    std::cout.flush();

    // map of order id to order, sorted automatically;
    std::map<int, std::shared_ptr<Order>> orders;

    // receive loop, server push update
    while (true) {

      Message msg = recv_msg(fd);

      switch (msg.get_type()) {

        case MessageType::DISP_ORDER: {
          
          auto order = msg.get_order();
          orders[order->get_id()] = order;
          print_all_orders(orders);
          break;
        }

        case MessageType::DISP_ITEM_UPDATE: {
         
          int order_id = msg.get_order_id();
          int item_id = msg.get_item_id();
          ItemStatus status = msg.get_item_status();

          auto it = orders.find(order_id);

          if (it != orders.end()) {
            auto item = it->second->find_item(item_id);

            if (item) {
              item->set_status(status);
            }
          }

          print_all_orders(orders);
          break;
        }

        case MessageType::DISP_ORDER_UPDATE: {
          int order_id = msg.get_order_id();
          OrderStatus status = msg.get_order_status();

          auto it = orders.find(order_id);
        
          if (it != orders.end()) {

            it->second->set_status(status);
            if (status == OrderStatus::DELIVERED) {
              orders.erase(it);
            }
          }

          print_all_orders(orders);
          break;
        }

        case MessageType::DISP_HEARTBEAT:
          // do nothing
          break;

        default:
          throw ProtocolError("Unexpected message type for display client");

      }

    }

  } catch (IOException &e) {

    std::cerr << "Error: " << e.what() << "\n";
    close(fd);
    return 1;

  } catch (InvalidMessage &e) {

    std::cerr << "Error: " << e.what() << "\n";
    close(fd);
    return 1;

  } catch (std::exception &e) {

    std::cerr << "Error: " << e.what() << "\n";
    close(fd);
    return 1;
    
  }

  close(fd);
  return 0;
}
