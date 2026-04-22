#ifndef CLIENT_UTIL_H
#define CLIENT_UTIL_H

#include "message.h"

//send a Message over give file descript
void send_msg(const Message &msg, int fd);

//receive and return Msg from the given file descr.
Message recv_msg(int fd);

#endif // CLIENT_UTIL_H
