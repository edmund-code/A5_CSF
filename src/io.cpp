#include <sstream>
#include <iomanip>
#include <cassert>
#include "csapp.h"
#include "except.h"
#include "io.h"

namespace {

// write exactly n bytes or throw IOException on short write/error
void write_exact(int fd, const char *buf, size_t n) {
  ssize_t written = rio_writen(fd, buf, n);
  if (written != ssize_t(n)) {
    throw IOException("error writing framed message");
  }
}

// read exactly n bytes or throw IOException on short read/error/EOF
void read_exact(int fd, char *buf, size_t n) {
  ssize_t num_read = rio_readn(fd, buf, n);
  if (num_read != ssize_t(n)) {
    throw IOException("error reading framed message");
  }
}

// parse a 4-byte decimal length prefix
int parse_len_prefix(const char prefix[4]) {
  for (int i = 0; i < 4; ++i) {
    if (prefix[i] < '0' || prefix[i] > '9') {
      throw IOException("invalid frame length prefix");
    }
  }

  std::string len_str(prefix, prefix + 4);
  int len = 0;
  try {
    len = std::stoi(len_str);
  } catch (...) {
    throw IOException("invalid frame length prefix");
  }

  if (len <= 0) {
    throw IOException("invalid frame length");
  }

  return len;
}


} // end of anonymous namespace for helper functions

namespace IO {
// frame and write one encoded message string to the output descriptor
void send(const std::string &s, int outfd) {
  // TODO: implement
  size_t framed_len = s.size() + 1;
  if (framed_len > 9999) {
    throw IOException("message too long to frame");
  }

  std::ostringstream out;
  out << std::setw(4) << std::setfill('0') << framed_len << s << '\n';
  std::string framed = out.str();

  write_exact(outfd, framed.data(), framed.size());
}

// read one framed message string from the input descriptor
void receive(int infd, std::string &s) {
  // TODO: implement
  char prefix[4];
  read_exact(infd, prefix, sizeof(prefix));
  int len = parse_len_prefix(prefix);

  std::string payload(size_t(len), '\0');
  read_exact(infd, payload.data(), payload.size());

  if (payload.empty() || payload.back() != '\n') {
    throw IOException("framed message missing trailing newline");
  }

  s = payload.substr(0, payload.size() - 1);
}

} // end of IO namespace
