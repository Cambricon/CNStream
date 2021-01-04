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

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "ipc_handler.hpp"
#include "module_ipc.hpp"

namespace cnstream {

class IPCHandlerTest : public IPCHandler {
 public:
  explicit IPCHandlerTest(const IPCType& type, ModuleIPC* ipc_module) : IPCHandler(type, ipc_module) {}
  ~IPCHandlerTest() {}
  bool Open() { return true; }
  void Close() {}
  void Shutdown() {}
  void RecvPackageLoop() {}
  bool Send(const std::string& send_str) { return true; }
  void SendPackageLoop() {}
};

TEST(IPCHandler, Construct) {
  std::shared_ptr<ModuleIPC> ipc = std::make_shared<ModuleIPC>("ipc");
  auto handler = std::make_shared<IPCHandlerTest>(IPC_CLIENT, ipc.get());
  EXPECT_TRUE(handler != nullptr);
}

TEST(IPCHandler, ParseStringToPackage) {
  std::shared_ptr<ModuleIPC> ipc = std::make_shared<ModuleIPC>("ipc");
  auto handler = std::make_shared<IPCHandlerTest>(IPC_CLIENT, ipc.get());
  std::string json_str = "{\"pkg_type\":2}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, nullptr));

  FrameInfoPackage msg_pack;
  EXPECT_TRUE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str = "{\"pkg_type\":3}";
  EXPECT_TRUE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str = "{\"pkg_type\":1,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0}";
  EXPECT_TRUE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_TRUE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[\"str\",1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":\"string\",\"dev_"
      "id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"mem_map_type\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mlu_mem_handle\":\"0\"}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));

  json_str =
      "{\"pkg_type\":0,\"stream_id\":\"0\",\"stream_idx\":0,\"frame_id\":0,\"flags\":0,\"timestamp\":0,\"data_fmt\":0,"
      "\"width\":1920,\"height\":1080,\"strides\":[1920,1920],\"dev_type\":0,\"dev_id\":0,"
      "\"ddr_channel\":0,\"mem_map_type\":0}";
  EXPECT_FALSE(handler->ParseStringToPackage(json_str, &msg_pack));
}

TEST(IPCHandler, SerializeToString) {
  std::shared_ptr<ModuleIPC> ipc = std::make_shared<ModuleIPC>("ipc");
  auto handler = std::make_shared<IPCHandlerTest>(IPC_CLIENT, ipc.get());
  std::string str;
  FrameInfoPackage pkg;
  EXPECT_FALSE(handler->SerializeToString(pkg, nullptr));

  pkg.pkg_type = PKG_ERROR;
  EXPECT_TRUE(handler->SerializeToString(pkg, &str));

  pkg.pkg_type = PKG_EXIT;
  EXPECT_TRUE(handler->SerializeToString(pkg, &str));

  pkg.pkg_type = PKG_RELEASE_MEM;
  pkg.stream_idx = 0;
  pkg.frame_id = 0;
  pkg.stream_id = "0";
  EXPECT_TRUE(handler->SerializeToString(pkg, &str));

  pkg.pkg_type = PKG_DATA;
  pkg.flags = 0;
  pkg.timestamp = 0;
  pkg.fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  pkg.width = 1920;
  pkg.height = 1080;
  pkg.stride[0] = 1920;
  pkg.stride[1] = 1920;
  pkg.ptr_mlu[0] = nullptr;
  pkg.ptr_mlu[1] = nullptr;
  pkg.ctx.dev_type = DevContext::DevType::MLU;
  pkg.ctx.dev_id = 0;
  pkg.ctx.ddr_channel = 0;
  pkg.mem_map_type = MEMMAP_MLU;
  pkg.mlu_mem_handle = nullptr;
  EXPECT_TRUE(handler->SerializeToString(pkg, &str));
}

TEST(IPCHandler, PreparePackageToSend) {
  std::shared_ptr<ModuleIPC> ipc = std::make_shared<ModuleIPC>("ipc");
  auto handler = std::make_shared<IPCHandlerTest>(IPC_CLIENT, ipc.get());

  std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create("0");
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->datas[CNDataFramePtrKey] = frame;

  EXPECT_NO_THROW(handler->PreparePackageToSend(PkgType::PKG_DATA, nullptr));
  EXPECT_NO_THROW(handler->PreparePackageToSend(PkgType::PKG_RELEASE_MEM, nullptr));
  EXPECT_NO_THROW(handler->PreparePackageToSend(PkgType::PKG_ERROR, nullptr));
  EXPECT_NO_THROW(handler->PreparePackageToSend(PkgType::PKG_EXIT, nullptr));
  EXPECT_NO_THROW(handler->PreparePackageToSend(PkgType::PKG_INVALID, nullptr));
}

TEST(IPCHandler, PackageToCNData) {
  std::shared_ptr<ModuleIPC> ipc = std::make_shared<ModuleIPC>("ipc");
  auto handler = std::make_shared<IPCHandlerTest>(IPC_CLIENT, ipc.get());

  FrameInfoPackage pkg;
  pkg.pkg_type = PKG_DATA;
  pkg.flags = 1;
  pkg.timestamp = 0;
  pkg.fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  pkg.width = 1920;
  pkg.height = 1080;
  pkg.stride[0] = 1920;
  pkg.stride[1] = 1920;
  pkg.ptr_mlu[0] = nullptr;
  pkg.ptr_mlu[1] = nullptr;
  pkg.ctx.dev_type = DevContext::DevType::MLU;
  pkg.ctx.dev_id = 0;
  pkg.ctx.ddr_channel = 0;
  pkg.mem_map_type = MEMMAP_MLU;
  pkg.mlu_mem_handle = nullptr;
  EXPECT_NO_THROW(handler->PackageToCNData(pkg, nullptr));

  std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create("0");
  EXPECT_NO_THROW(handler->PackageToCNData(pkg, data));
}
}  // namespace cnstream
