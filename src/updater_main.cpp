//main program for the updater client

#include <iostream>
#include <string>
#include <stdexcept>
#include "csapp.h"
#include "model.h"
#include "message.h"
#include "wire.h"
#include "io.h"
#include "except.h"
#include "util.h"
#include "client_util.h"

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

    //get login credentials from user
    std::string username, password;
    std::cout << "username: ";

    std::getline(std::cin, username);
    std::cout << "password: ";

    std::getline(std::cin, password);

    std::string creds = username + "/" + password;

    //send LOGIN as updater
    send_msg(Message(MessageType::LOGIN, ClientMode::UPDATER, creds), fd);

    //check server response
    Message resp = recv_msg(fd);
    if (resp.get_type() == MessageType::ERROR) {

      std::cerr << "Error: " << resp.get_str() << "\n";
      close(fd);

      return 1;
    }

    //main command loop
    while (true) {

      std::string cmd;
      std::cout << "> ";
      std::getline(std::cin, cmd);

      if (cmd == "quit") {

        send_msg(Message(MessageType::QUIT), fd);
        recv_msg(fd);  // recv OK
        break;

      } else if (cmd == "order_new") {

        //read number of items
        std::string line;
        std::getline(std::cin, line);
        int num_items = std::stoi(line);

        auto order = std::make_shared<Order>(1, OrderStatus::NEW);

        for (int i = 0; i < num_items; i++) {

          std::string item_id_str, desc, qty_str;
          std::getline(std::cin, item_id_str);
          std::getline(std::cin, desc);

          std::getline(std::cin, qty_str);

          int item_id = std::stoi(item_id_str);
          int qty = std::stoi(qty_str);

          order->add_item(std::make_shared<Item>(1, item_id, ItemStatus::NEW, desc, qty));
        }

        send_msg(Message(MessageType::ORDER_NEW, order), fd);

        Message r = recv_msg(fd);

        if (r.get_type() == MessageType::OK) {

          std::cout << "Success: " << r.get_str() << "\n";
        } else {

          std::cout << "Failure: " << r.get_str() << "\n";
        }

      } else if (cmd == "item_update") {

        std::string order_id_str, item_id_str, status_str;
        std::getline(std::cin, order_id_str);
        std::getline(std::cin, item_id_str);
        std::getline(std::cin, status_str);

        int order_id = std::stoi(order_id_str);
        int item_id = std::stoi(item_id_str);
        ItemStatus status = Wire::str_to_item_status(status_str);

        send_msg(Message(MessageType::ITEM_UPDATE, order_id, item_id, status), fd);

        Message r = recv_msg(fd);

        if (r.get_type() == MessageType::OK) {

          std::cout << "Success: " << r.get_str() << "\n";
        } else {

          std::cout << "Failure: " << r.get_str() << "\n";
        }

      } else if (cmd == "order_update") {

        std::string order_id_str, status_str;
        std::getline(std::cin, order_id_str);
        
        std::getline(std::cin, status_str);

        int order_id = std::stoi(order_id_str);
        OrderStatus status = Wire::str_to_order_status(status_str);

        send_msg(Message(MessageType::ORDER_UPDATE, order_id, status), fd);

        Message r = recv_msg(fd);

        if (r.get_type() == MessageType::OK) {

          std::cout << "Success: " << r.get_str() << "\n";

        } else {

          std::cout << "Failure: " << r.get_str() << "\n";
        }

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
