#include <string>
#include <unordered_map>
#include <cassert>
#include "util.h"
#include "model.h"
#include "except.h"
#include "wire.h"

namespace {

// private data and helper functions

const std::unordered_map<ClientMode, std::string> s_client_mode_to_str = {
  { ClientMode::INVALID, "INVALID" },
  { ClientMode::UPDATER, "UPDATER" },
  { ClientMode::DISPLAY, "DISPLAY" },
};

const std::vector<std::pair<std::string, ClientMode>> s_str_to_client_mode_vec =
  Util::invert_unordered_map(s_client_mode_to_str);

const std::unordered_map<std::string, ClientMode> s_str_to_client_mode(
  s_str_to_client_mode_vec.begin(),
  s_str_to_client_mode_vec.end()
);

const std::unordered_map<MessageType, std::string> s_message_type_to_str = {
  { MessageType::INVALID, "INVALID" },
  { MessageType::LOGIN, "LOGIN" },
  { MessageType::QUIT, "QUIT" },
  { MessageType::ORDER_NEW, "ORDER_NEW" },
  { MessageType::ITEM_UPDATE, "ITEM_UPDATE" },
  { MessageType::ORDER_UPDATE, "ORDER_UPDATE" },
  { MessageType::OK, "OK" },
  { MessageType::ERROR, "ERROR" },
  { MessageType::DISP_ORDER, "DISP_ORDER" },
  { MessageType::DISP_ITEM_UPDATE, "DISP_ITEM_UPDATE" },
  { MessageType::DISP_ORDER_UPDATE, "DISP_ORDER_UPDATE" },
  { MessageType::DISP_HEARTBEAT, "DISP_HEARTBEAT" },
};

const std::vector<std::pair<std::string, MessageType>> s_str_to_message_type_vec =
  Util::invert_unordered_map(s_message_type_to_str);

const std::unordered_map<std::string, MessageType> s_str_to_message_type(
  s_str_to_message_type_vec.begin(),
  s_str_to_message_type_vec.end()
);

const std::unordered_map<ItemStatus, std::string> s_item_status_to_str = {
  { ItemStatus::INVALID, "INVALID" },
  { ItemStatus::NEW, "NEW" },
  { ItemStatus::IN_PROGRESS, "IN_PROGRESS" },
  { ItemStatus::DONE, "DONE" },
};

const std::vector<std::pair<std::string, ItemStatus>> s_str_to_item_status_vec =
  Util::invert_unordered_map(s_item_status_to_str);

const std::unordered_map<std::string, ItemStatus> s_str_to_item_status(
  s_str_to_item_status_vec.begin(),
  s_str_to_item_status_vec.end()
);

const std::unordered_map<OrderStatus, std::string> s_order_status_to_str = {
  { OrderStatus::INVALID, "INVALID" },
  { OrderStatus::NEW, "NEW" },
  { OrderStatus::IN_PROGRESS, "IN_PROGRESS" },
  { OrderStatus::DONE, "DONE" },
  { OrderStatus::DELIVERED, "DELIVERED" },
};

const std::vector<std::pair<std::string, OrderStatus>> s_str_to_order_status_vec =
  Util::invert_unordered_map(s_order_status_to_str);

const std::unordered_map<std::string, OrderStatus> s_str_to_order_status(
  s_str_to_order_status_vec.begin(),
  s_str_to_order_status_vec.end()
);

// encode a single item into the colon separated payload form used by orders
std::string encode_item(const Item &item) {
  return std::to_string(item.get_order_id()) + ":" + std::to_string(item.get_id()) + ":" + s_item_status_to_str.at(item.get_status()) + ":" + item.get_desc() + ":" + std::to_string(item.get_qty());
}

// encode an order and all of its items into the comma semicolon separated payload form
std::string encode_order(const std::shared_ptr<Order> &order) {
  assert(order);
  if (order->get_num_items() == 0)
    throw InvalidMessage("order must contain at least one item");
  std::string result = std::to_string(order->get_id()) + "," + s_order_status_to_str.at(order->get_status()) + ",";

  for (int i = 0; i < order->get_num_items(); ++i) {
    if (i > 0)
      result += ";";
    result += encode_item(*order->at(i));
  }

  return result;
}

// parse a base 10 integer and reject partial parses or other malformed values
int parse_int(const std::string &s) {
  if (s.empty())
    throw InvalidMessage("invalid integer");

  size_t pos = 0;
  int value = 0;

  try {
    value = std::stoi(s, &pos, 10);
  } catch (...) {
    throw InvalidMessage("invalid integer");
  }

  if (pos != s.size())
    throw InvalidMessage("invalid integer");

  return value;
}

// Parse a strictly positive integer.
int parse_positive_int(const std::string &s) {
  int value = parse_int(s);
  if (value <= 0)
    throw InvalidMessage("invalid positive integer");
  return value;
}

// decode a single item from the colon-separated payload form
std::shared_ptr<Item> decode_item(const std::string &s) {
  auto parts = Util::split(s, ':');
  if (parts.size() != 5)
    throw InvalidMessage("invalid item encoding");
  int order_id = parse_positive_int(parts[0]);
  int item_id = parse_positive_int(parts[1]);
  auto item_status_it = s_str_to_item_status.find(parts[2]);
  ItemStatus status = ItemStatus::INVALID;
  if (item_status_it != s_str_to_item_status.end())
    status = item_status_it->second;
  if (status == ItemStatus::INVALID)
    throw InvalidMessage("invalid item status");
  int qty = parse_positive_int(parts[4]);
  return std::make_shared<Item>(order_id, item_id, status, parts[3], qty);
}

// decode an order and its items from the comma/semicolon separated payload form
std::shared_ptr<Order> decode_order(const std::string &s) {
  auto parts = Util::split(s, ',');
  if (parts.size() != 3)
    throw InvalidMessage("invalid order encoding");

  int order_id = parse_positive_int(parts[0]);
  auto order_status_it = s_str_to_order_status.find(parts[1]);
  OrderStatus status = OrderStatus::INVALID;
  if (order_status_it != s_str_to_order_status.end())
    status = order_status_it->second;
  if (status == OrderStatus::INVALID)
    throw InvalidMessage("invalid order status");

  if (parts[2].empty())
    throw InvalidMessage("order must contain at least one item");

  auto order = std::make_shared<Order>(order_id, status);
  auto item_strs = Util::split(parts[2], ';');
  for (const auto &item_str : item_strs)
    order->add_item(decode_item(item_str));
  return order;
}


} // end of anonymous namespace for helper functions

namespace Wire {

std::string client_mode_to_str(ClientMode client_mode) {
  auto i = s_client_mode_to_str.find(client_mode);
  if (i != s_client_mode_to_str.end())
    return i->second;
  return "<invalid>";
}

ClientMode str_to_client_mode(const std::string &s) {
  auto i = s_str_to_client_mode.find(s);
  if (i != s_str_to_client_mode.end())
    return i->second;
  return ClientMode::INVALID;
}

std::string message_type_to_str(MessageType message_type) {
  auto i = s_message_type_to_str.find(message_type);
  if (i != s_message_type_to_str.end())
    return i->second;
  return "<invalid>";
}

MessageType str_to_message_type(const std::string &s) {
  auto i = s_str_to_message_type.find(s);
  if (i != s_str_to_message_type.end())
    return i->second;
  return MessageType::INVALID;
}

std::string item_status_to_str(ItemStatus item_status) {
  auto i = s_item_status_to_str.find(item_status);
  if (i != s_item_status_to_str.end())
    return i->second;
  return "<invalid>";
}

ItemStatus str_to_item_status(const std::string &s) {
  auto i = s_str_to_item_status.find(s);
  if (i != s_str_to_item_status.end())
    return i->second;
  return ItemStatus::INVALID;
}

std::string order_status_to_str(OrderStatus order_status) {
  auto i = s_order_status_to_str.find(order_status);
  if (i != s_order_status_to_str.end())
    return i->second;
  return "<invalid>";
}

OrderStatus str_to_order_status(const std::string &s) {
  auto i = s_str_to_order_status.find(s);
  if (i != s_str_to_order_status.end())
    return i->second;
  return OrderStatus::INVALID;
}

void encode(const Message &msg, std::string &s) {
  s = message_type_to_str(msg.get_type());

  switch (msg.get_type()) {
    case MessageType::LOGIN:
      s += "|" + client_mode_to_str(msg.get_client_mode()) + "|" + msg.get_str();
      break;

    case MessageType::QUIT:
    case MessageType::OK:
    case MessageType::ERROR:
      s += "|" + msg.get_str();
      break;

    case MessageType::ORDER_NEW:
    case MessageType::DISP_ORDER:
      s += "|" + encode_order(msg.get_order());
      break;

    case MessageType::ITEM_UPDATE:
    case MessageType::DISP_ITEM_UPDATE:
      s += "|" + std::to_string(msg.get_order_id()) + "|" +
           std::to_string(msg.get_item_id()) + "|" +
           item_status_to_str(msg.get_item_status());
      break;

    case MessageType::ORDER_UPDATE:
    case MessageType::DISP_ORDER_UPDATE:
      s += "|" + std::to_string(msg.get_order_id()) + "|" +
           order_status_to_str(msg.get_order_status());
      break;

    case MessageType::DISP_HEARTBEAT:
    case MessageType::INVALID:
      break;
  }
}

void decode(const std::string &s, Message &msg) {
  auto parts = Util::split(s, '|');
  if (parts.empty())
    throw InvalidMessage("empty message");

  MessageType type = str_to_message_type(parts[0]);
  if (type == MessageType::INVALID)
    throw InvalidMessage("invalid message type");

  Message result;
  result.set_type(type);

  switch (type) {
    case MessageType::LOGIN:
      if (parts.size() != 3)
        throw InvalidMessage("invalid login message");
      {
        ClientMode client_mode = Wire::str_to_client_mode(parts[1]);
        if (client_mode == ClientMode::INVALID)
          throw InvalidMessage("invalid client mode");
        result.set_client_mode(client_mode);
        result.set_str(parts[2]);
      }
      break;

    case MessageType::QUIT:
    case MessageType::OK:
    case MessageType::ERROR:
      if (parts.size() != 2)
        throw InvalidMessage("invalid string message");
      result.set_str(parts[1]);
      break;

    case MessageType::ORDER_NEW:
    case MessageType::DISP_ORDER:
      if (parts.size() != 2)
        throw InvalidMessage("invalid order message");
      result.set_order(decode_order(parts[1]));
      break;

    case MessageType::ITEM_UPDATE:
    case MessageType::DISP_ITEM_UPDATE:
      if (parts.size() != 4)
        throw InvalidMessage("invalid item update message");
      result.set_order_id(parse_positive_int(parts[1]));
      result.set_item_id(parse_positive_int(parts[2]));
      {
        ItemStatus item_status = Wire::str_to_item_status(parts[3]);
        if (item_status == ItemStatus::INVALID)
          throw InvalidMessage("invalid item status");
        result.set_item_status(item_status);
      }
      break;

    case MessageType::ORDER_UPDATE:
    case MessageType::DISP_ORDER_UPDATE:
      if (parts.size() != 3)
        throw InvalidMessage("invalid order update message");
      result.set_order_id(parse_positive_int(parts[1]));
      {
        OrderStatus order_status = Wire::str_to_order_status(parts[2]);
        if (order_status == OrderStatus::INVALID)
          throw InvalidMessage("invalid order status");
        result.set_order_status(order_status);
      }
      break;

    case MessageType::DISP_HEARTBEAT:
      if (parts.size() != 1)
        throw InvalidMessage("invalid heartbeat message");
      break;

    case MessageType::INVALID:
      throw InvalidMessage("invalid message type");
  }

  msg = result;
}

}
