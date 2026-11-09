#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <errno.h>
#include <filesystem>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "shared/error.h"
#include "shared/io.h"
#include "shared/protocol.h"

static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
  uint32_t len = 4;
  for (const std::string &s : cmd) {
    len = len + 4 + s.size();
  }
  if (len > K_MAX_MSG) {
    return -1;
  }

  char wbuf[4 + K_MAX_MSG];
  memcpy(&wbuf[0], &len, 4);
  uint32_t n = cmd.size();
  memcpy(&wbuf[4], &n, 4);
  size_t cur = 8;
  for (const std::string &s : cmd) {
    uint32_t p = (uint32_t)s.size();
    memcpy(&wbuf[cur], &p, 4);
    memcpy(&wbuf[cur + 4], s.data(), s.size());
    cur = cur + 4 + s.size();
  }
  return write_all(fd, wbuf, 4 + len);
}

static int32_t print_response(const uint8_t *data, size_t size) {
  if (size < 1) {
    msg("bad response");
    return -1;
  }
  switch (data[0]) {
  case TAG_NIL:
    printf("(nil)\n");
    return 1;
  case TAG_ERR:
    if (size < 1 + 8) {
      msg("bad response");
      return -1;
    }
    {
      int32_t code = 0;
      uint32_t len = 0;
      memcpy(&code, &data[1], 4);
      memcpy(&len, &data[1 + 4], 4);
      if (size < 1 + 8 + len) {
        msg("bad response");
        return -1;
      }
      printf("(err) %d %.*s\n", code, len, &data[1 + 8]);
      return 1 + 8 + len;
    }
  case TAG_STR:
    if (size < 1 + 4) {
      msg("bad response");
      return -1;
    }
    {
      uint32_t len = 0;
      memcpy(&len, &data[1], 4);
      if (size < 1 + 4 + len) {
        msg("bad response");
        return -1;
      }
      printf("(str) %.*s\n", len, &data[1 + 4]);
      return 1 + 4 + len;
    }
  case TAG_INT:
    if (size < 1 + 8) {
      msg("bad response");
      return -1;
    }
    {
      int64_t val = 0;
      memcpy(&val, &data[1], 8);
      printf("(int) %ld\n", val);
      return 1 + 8;
    }
  case TAG_DBL:
    if (size < 1 + 8) {
      msg("bad response");
      return -1;
    }
    {
      double val = 0;
      memcpy(&val, &data[1], 8);
      printf("(dbl) %g\n", val);
      return 1 + 8;
    }
  case TAG_ARR:
    if (size < 1 + 4) {
      msg("bad response");
      return -1;
    }
    {
      uint32_t len = 0;
      memcpy(&len, &data[1], 4);
      printf("(arr) len=%u\n", len);
      size_t arr_bytes = 1 + 4;
      for (uint32_t i = 0; i < len; ++i) {
        int32_t rv = print_response(&data[arr_bytes], size - arr_bytes);
        if (rv < 0) {
          return rv;
        }
        arr_bytes = arr_bytes + (size_t)rv;
      }
      printf("(arr) end\n");
      return (int32_t)arr_bytes;
    }
  default:
    msg("bad response");
    return -1;
  }
}

static int32_t read_res(int fd) {
  char rbuf[4 + K_MAX_MSG];
  errno = 0;
  int32_t err = read_full(fd, rbuf, 4);
  if (err) {
    if (errno == 0) {
      msg("EOF");
    } else {
      msg("read() error");
    }
    return err;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4);
  if (len > K_MAX_MSG) {
    msg("too long");
    return -1;
  }

  err = read_full(fd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }

  // uint32_t rescode = 0;
  // if (len < 4) {
  //     msg("bad response");
  //     return -1;
  // }
  // memcpy(&rescode, &rbuf[4], 4);
  // if (len - 4  > 0) {
  //     printf("server says: status: %u, data: %.*s\n", rescode, len - 4,
  //     &rbuf[8]);
  // } else {
  //     printf("server says: status: %u\n", rescode);
  // }
  int32_t rv = print_response((uint8_t *)&rbuf[4], len);
  if (rv > 0 && (uint32_t)rv != len) {
    msg("bad response");
    rv = -1;
  }
  return rv;
}

int main(int argc, char *argv[]) {
  const char *value = getenv("REDSTONE_HOST");
  if (!value) {
    value = "127.0.0.1";
  }
  /* Source: man socket.2
   * AF_INET      use IPv4 internet protocols
   *
   * SOCK_STREAM  use sequenced, reliable, two-way, connection-based streams
   *
   * socket()     creates a communication endpoint and a file descriptor that
   * refers to said communication endpoint
   */
  int fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET + SOCK_STREAM = TCP
  if (fd < 0) {
    die("socket()");
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;                          // IPv4 Internet Protocol
  addr.sin_port = ntohs(1234);                        // Port 1234
                                                      //
  int rv = inet_pton(AF_INET, value, &addr.sin_addr); // IP 127.0.0.1
  if (rv == 1) {
    // msg("inet_pton() successfully converted ip string");
  } else if (rv == 0) {
    die("inet_pton() invalid ip string");
  } else {
    die("inet_pton() inet_pton failed");
  }

  /* Source: man connect.2
   * connect()    connects the file descriptor to an address
   */
  if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
    die("connect()");
  }

  std::vector<std::string> cmd;
  for (int i = 1; i < argc; ++i) {
    cmd.push_back(argv[i]);
  }

  int32_t err = send_req(fd, cmd);
  if (err) {
    goto L_DONE;
  }
  err = read_res(fd);
  if (err) {
    goto L_DONE;
  }

L_DONE:
  close(fd);
  return 0;
}
