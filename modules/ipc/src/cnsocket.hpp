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

#ifndef MODULES_IPC_CNSOCKET_HPP_
#define MODULES_IPC_CNSOCKET_HPP_

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <atomic>
#include <string>

namespace cnstream {

/**
 * @brief for CNSocket, encapsulate socket operation.
 */
class CNSocket {
 public:
  /**
   *  @brief  Open communicate source.
   *  @return Return true if open communicate source successfully, otherwise, return false.
   */
  virtual bool Open(const std::string& socket_address) { return false; }

  /**
   *  @brief  Close communicate source.
   *  @return Void.
   */
  void Close();

  /**
  *  @brief  Shutdown communicate connection.
  *  @return Void.
  */
  void Shutdown();

  /**
  *  @brief  Receive data from socket fd.
  *  @return received data bytes.
  */
  int RecvData(char* buf, int buf_size);

  /**
  *  @brief  Send data to socket fd.
  *  @return send data bytes.
  */
  int SendData(char* buf, int buf_size);

  std::string socket_addr_;  // communicate socket address
  int socket_fd_ = -1;       // socket fd to read and write
};

/**
 * @brief for CNServer.
 */
class CNServer : public CNSocket {
 public:
  /**
   *  @brief  Open server source.
   *  @return Return true if open server source successfully, otherwise, return false.
   */
  bool Open(const std::string& socket_address) override;

  /**
  *  @brief  Accept client to connect.
  *  @return return client fd if any client connect successfully, otherwise, return -1.
  */
  int Accept();

  /**
  *  @brief Close server listening.
  *  @return Void.
  */
  void CloseListen();

 private:
  int listen_fd_ = -1;  // listen socket fd for server
};

/**
 * @brief for CNClient.
 */
class CNClient : public CNSocket {
 public:
  /**
   *  @brief  Open client source.
   *  @return Return true if open client source successfully, otherwise, return false.
   */
  bool Open(const std::string& socket_address) override;
};

}  //  namespace cnstream

#endif
