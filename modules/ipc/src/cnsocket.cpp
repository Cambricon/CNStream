/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/

#include <unistd.h>
#include <memory>
#include <string>

#include "cnsocket.hpp"
#include "glog/logging.h"

namespace cnstream {

void CNSocket::Close() {
  close(socket_fd_);

  if (access(socket_addr_.c_str(), F_OK) == 0) {
    unlink(socket_addr_.c_str());
  }
}

void CNSocket::Shutdown() { shutdown(socket_fd_, SHUT_RDWR); }

int CNSocket::RecvData(char* read_buf, int buf_size) {
  if (nullptr != read_buf) {
    return recv(socket_fd_, read_buf, buf_size, 0);
  }

  return -1;
}

int CNSocket::SendData(char* send_buf, int buf_size) {
  if (nullptr != send_buf) {
    return send(socket_fd_, send_buf, buf_size, 0);
  }

  return -1;
}

bool CNServer::Open(const std::string& socket_address) {
  listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (-1 == listen_fd_) {
    LOG(ERROR) << "create listen_fd for server failed, errno: " << errno;
    return false;
  }

  socket_addr_ = socket_address;
  if (access(socket_addr_.c_str(), F_OK) == 0) {
    LOG(INFO) << socket_addr_ << " exists, unlink it.";
    unlink(socket_addr_.c_str());
  }

  sockaddr_un un;
  memset(&un, 0, sizeof(un));
  un.sun_family = AF_UNIX;
  memcpy(un.sun_path, socket_addr_.c_str(), socket_addr_.length());
  unsigned int length = strlen(un.sun_path) + sizeof(un.sun_family);
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&un), length) < 0) {
    LOG(ERROR) << "bind server listen_fd failed, errno: " << errno;
    return false;
  }

  if (listen(listen_fd_, 1) < 0) {
    LOG(ERROR) << "start server listen failed, errno: " << errno;
    return false;
  }

  return true;
}

int CNServer::Accept() {
  if (listen_fd_ < 0) {
    LOG(ERROR) << "server is not listening.";
    return -1;
  }

  sockaddr_un un;
  unsigned int length = sizeof(un);
  socket_fd_ = accept(listen_fd_, reinterpret_cast<sockaddr*>(&un), &length);
  if (-1 == socket_fd_) {
    LOG(ERROR) << "server accept failed, errno: " << errno;
    return -1;
  }

  return socket_fd_;
}

void CNServer::CloseListen() {
  close(listen_fd_);
  listen_fd_ = -1;
}

bool CNClient::Open(const std::string& socket_address) {
  socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (-1 == socket_fd_) {
    LOG(ERROR) << "create client socket fd failed, errno: " << errno;
    return false;
  }

  socket_addr_ = socket_address;
  if (access(socket_addr_.c_str(), F_OK) < 0) {
    LOG(WARNING) << socket_addr_ << " not exists, can not create connection.";
    return false;
  }

  sockaddr_un un;
  memset(&un, 0, sizeof(un));
  un.sun_family = AF_UNIX;
  memcpy(un.sun_path, socket_addr_.c_str(), socket_addr_.length());
  unsigned length = strlen(un.sun_path) + sizeof(un.sun_family);

  if (connect(socket_fd_, reinterpret_cast<sockaddr*>(&un), length) < 0) {
    close(socket_fd_);
    socket_fd_ = -1;
    LOG(ERROR) << "client connect failed, errno: " << errno;
    return false;
  }

  return true;
}

}  //  namespace cnstream
