#include "client_util.h"
#include "wire.h"
#include "io.h"

//encode the message and send it over fd
void send_msg(const Message &msg, int fd) {

  std::string s;
  Wire::encode(msg, s);
  IO::send(s, fd);

}

//receive a raw frame from fd and decode it into a msg
Message recv_msg(int fd) {

  std::string s;
  IO::receive(fd, s);

  Message msg;
  Wire::decode(s, msg);

  return msg;

}
