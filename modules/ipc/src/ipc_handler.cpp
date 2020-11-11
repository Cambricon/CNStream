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

#include <glog/logging.h>

#include <fcntl.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <string>

#include "ipc_handler.hpp"

namespace cnstream {

bool IPCHandler::OpenSemphore() {
  std::string sem_name = "sem_" + socket_address_;
  // if semaphore exists, open and use directly
  sem_id_ = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0644, 0);
  if (sem_id_ == SEM_FAILED) {
    if (errno == EEXIST) {
      LOG(INFO) << "semaphore: " << sem_name << " exist, open it directly.";
      sem_id_ = sem_open(sem_name.c_str(), 0);
      if (sem_id_ == SEM_FAILED) {
        LOG(ERROR) << "semaphore: " << sem_name << " open error.";
        return false;
      }
    }
  } else {
    LOG(INFO) << "semaphore: " << sem_name << " create successfully.";
    sem_created_ = true;
  }

  return true;
}

void IPCHandler::CloseSemphore() {
  std::string sem_name = "sem_" + socket_address_;
  sem_close(sem_id_);
  if (sem_created_) sem_unlink(sem_name.c_str());
}

bool IPCHandler::PostSemphore() {
  if (sem_id_) {
    int ret = sem_post(sem_id_);
    return ret == 0;
  }

  return false;
}

bool IPCHandler::WaitSemphore() {
  if (sem_id_) {
    int ret = sem_wait(sem_id_);
    return ret == 0;
  }

  return false;
}

bool IPCHandler::ParseStringToPackage(const std::string& str, FrameInfoPackage* pkg) {
  if (!pkg) return false;

  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(str.c_str()).HasParseError()) {
    LOG(ERROR) << "SerializeFromString failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << str;
    return false;
  }

  // get members
  const auto end = doc.MemberEnd();

  // pkg_type
  if (end == doc.FindMember("pkg_type") || !doc["pkg_type"].IsInt()) {
    return false;
  } else {
    pkg->pkg_type = PkgType(doc["pkg_type"].GetInt());
  }

  if (PKG_RELEASE_MEM == pkg->pkg_type || PKG_DATA == pkg->pkg_type) {
    if (end == doc.FindMember("stream_id") || !doc["stream_id"].IsString()) {
      LOG(WARNING) << "parse stream_id error.";
      return false;
    } else {
      pkg->stream_id = doc["stream_id"].GetString();
    }

    if (end == doc.FindMember("stream_idx") || !doc["stream_idx"].IsUint()) {
      LOG(WARNING) << "parse stream_idx error.";
      return false;
    } else {
      pkg->stream_idx = doc["stream_idx"].GetUint();
    }

    if (end == doc.FindMember("frame_id") || !doc["frame_id"].IsInt64()) {
      LOG(WARNING) << "parse frame_id error.";
      return false;
    } else {
      pkg->frame_id = doc["frame_id"].GetInt64();
    }
  }

  if (PKG_DATA == pkg->pkg_type) {
    if (end == doc.FindMember("flags") || !doc["flags"].IsUint()) {
      LOG(WARNING) << "parse flags error.";
      return false;
    } else {
      pkg->flags = doc["flags"].GetUint();
    }

    if (end == doc.FindMember("timestamp") || !doc["timestamp"].IsInt64()) {
      LOG(WARNING) << "parse timestamp error.";
      return false;
    } else {
      pkg->timestamp = doc["timestamp"].GetInt64();
    }

    if (end == doc.FindMember("data_fmt") || !doc["data_fmt"].IsInt()) {
      LOG(WARNING) << "parse data fmt error.";
      return false;
    } else {
      pkg->fmt = CNDataFormat(doc["data_fmt"].GetInt());
    }

    if (end == doc.FindMember("width") || !doc["width"].IsInt()) {
      LOG(WARNING) << "parse width error.";
      return false;
    } else {
      pkg->width = doc["width"].GetInt();
    }

    if (end == doc.FindMember("height") || !doc["height"].IsInt()) {
      LOG(WARNING) << "parse height error.";
      return false;
    } else {
      pkg->height = doc["height"].GetInt();
    }

    if (end == doc.FindMember("strides") || !doc["strides"].IsArray()) {
      LOG(WARNING) << "parse strides error.";
      return false;
    } else {
      auto values = doc["strides"].GetArray();
      int i = 0;
      for (auto iter = values.begin(); iter != values.end(); ++iter) {
        if (!iter->IsInt()) {
          LOG(WARNING) << "parse strides type error.";
          return false;
        }
        pkg->stride[i] = iter->GetInt();
        i++;
      }
    }

    if (end == doc.FindMember("dev_type") || !doc["dev_type"].IsInt()) {
      LOG(WARNING) << "parse dev_type error.";
      return false;
    } else {
      pkg->ctx.dev_type = DevContext::DevType(doc["dev_type"].GetInt());
    }

    if (end == doc.FindMember("dev_id") || !doc["dev_id"].IsInt()) {
      LOG(WARNING) << "parse dev_id error.";
      return false;
    } else {
      pkg->ctx.dev_id = doc["dev_id"].GetInt();
    }

    if (end == doc.FindMember("ddr_channel") || !doc["ddr_channel"].IsInt()) {
      LOG(WARNING) << "parse ddr_channel error.";
      return false;
    } else {
      pkg->ctx.ddr_channel = doc["ddr_channel"].GetInt();
    }

    if (end == doc.FindMember("mem_map_type") || !doc["mem_map_type"].IsInt()) {
      LOG(WARNING) << "parse mem_map_type error.";
      return false;
    } else {
      pkg->mem_map_type = MemMapType(doc["mem_map_type"].GetInt());
    }

    if (end == doc.FindMember("mlu_mem_handle") || !doc["mlu_mem_handle"].IsString()) {
      LOG(WARNING) << "parse mlu_mem_handle error.";
      return false;
    } else {
      try {
        intptr_t tmp = std::stoll(doc["mlu_mem_handle"].GetString());
        pkg->mlu_mem_handle = reinterpret_cast<void*>(tmp);
      } catch (const std::invalid_argument& e) {
        LOG(WARNING) << "mlu_mem_handle is invalid.";
        return false;
      }
    }
  }
  return true;
}

bool IPCHandler::SerializeToString(const FrameInfoPackage& pkg, std::string* str) {
  if (!str) return false;
  rapidjson::StringBuffer strBuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
  writer.StartObject();

  writer.Key("pkg_type");
  writer.Int(static_cast<int>(pkg.pkg_type));

  if (PKG_DATA == pkg.pkg_type || PKG_RELEASE_MEM == pkg.pkg_type) {
    writer.Key("stream_idx");
    writer.Uint(pkg.stream_idx);

    writer.Key("stream_id");
    writer.String(pkg.stream_id.c_str());

    writer.Key("frame_id");
    writer.Int64(pkg.frame_id);
  }

  if (PKG_DATA == pkg.pkg_type) {
    writer.Key("flags");
    writer.Uint(pkg.flags);

    writer.Key("timestamp");
    writer.Int64(pkg.timestamp);

    writer.Key("data_fmt");
    writer.Int(static_cast<int>(pkg.fmt));

    writer.Key("width");
    writer.Int(pkg.width);

    writer.Key("height");
    writer.Int(pkg.height);

    writer.Key("strides");
    writer.StartArray();
    for (int i = 0; i < CN_MAX_PLANES; i++) {
      writer.Int(pkg.stride[i]);
    }
    writer.EndArray();

    writer.Key("dev_type");
    writer.Int(static_cast<int>(pkg.ctx.dev_type));

    writer.Key("dev_id");
    writer.Int(pkg.ctx.dev_id);

    writer.Key("ddr_channel");
    writer.Int(pkg.ctx.ddr_channel);

    writer.Key("mem_map_type");
    writer.Int(static_cast<int>(pkg.mem_map_type));

    writer.Key("mlu_mem_handle");
    intptr_t ptmp = reinterpret_cast<intptr_t>(pkg.mlu_mem_handle);
    writer.String(std::to_string(ptmp).c_str());
  }
  writer.EndObject();

  *str = strBuf.GetString();
  return true;
}

void IPCHandler::PreparePackageToSend(const PkgType& type, const std::shared_ptr<CNFrameInfo> data) {
  FrameInfoPackage send_pkg;
  switch (type) {
    case PkgType::PKG_DATA: {
      if (!data) {
        LOG(WARNING) << "frame data to pack data message is nullptr.";
        return;
      }

      send_pkg.pkg_type = PkgType::PKG_DATA;
      send_pkg.stream_idx = data->GetStreamIndex();
      send_pkg.stream_id = data->stream_id;
      send_pkg.flags = data->flags;
      send_pkg.timestamp = data->timestamp;
      send_pkg.mem_map_type = memmap_type_;
      if (!data->IsEos()) {
        CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
        send_pkg.frame_id = frame->frame_id;
        send_pkg.fmt = frame->fmt;
        send_pkg.width = frame->width;
        send_pkg.height = frame->height;

        for (int i = 0; i < frame->GetPlanes(); i++) {
          send_pkg.stride[i] = frame->stride[i];
        }
        send_pkg.mlu_mem_handle = frame->mlu_mem_handle;
        send_pkg.ctx.dev_type = frame->ctx.dev_type;
        send_pkg.ctx.dev_id = frame->ctx.dev_id;
        send_pkg.ctx.ddr_channel = frame->ctx.ddr_channel;
      }
    } break;
    case PkgType::PKG_RELEASE_MEM: {
      if (!data) {
        LOG(WARNING) << "frame data to release shared memory is nullptr.";
        return;
      }

      send_pkg.pkg_type = PkgType::PKG_RELEASE_MEM;
      send_pkg.stream_idx = data->GetStreamIndex();
      send_pkg.stream_id = data->stream_id;
      if (!data->IsEos()) {
        CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
        send_pkg.frame_id = frame->frame_id;
      }
    } break;
    case PkgType::PKG_ERROR:
      send_pkg.pkg_type = PkgType::PKG_ERROR;
      break;
    case PkgType::PKG_EXIT:
      send_pkg.pkg_type = PkgType::PKG_EXIT;
      break;
    default:
      LOG(WARNING) << "unsupported message type in ipc.";
      return;
  }

  if (IPC_CLIENT == ipc_type_) {
    std::string send_str;
    SerializeToString(send_pkg, &send_str);
    memset(send_buf_, 0, sizeof(send_buf_));
    memcpy(send_buf_, send_str.c_str(), send_str.length());
  } else if (IPC_SERVER == ipc_type_) {
    send_pkgq_.Push(send_pkg);
  }
}

void IPCHandler::PackageToCNData(const FrameInfoPackage& recv_pkg, std::shared_ptr<CNFrameInfo> data) {
  if (!data) {
    LOG(WARNING) << "frame data for trans message pack is nullptr, pack message frame id: " << recv_pkg.frame_id
                 << std::endl;
    return;
  }

  std::shared_ptr<CNDataFrame> dataframe(new (std::nothrow) CNDataFrame());
  if (!dataframe) {
    return;
  }
  data->flags = recv_pkg.flags;
  data->SetStreamIndex(recv_pkg.stream_idx);
  data->stream_id = recv_pkg.stream_id;
  data->timestamp = recv_pkg.timestamp;
  // sync shared memory for frame data
  if (data->IsEos()) {
    return;
  }

  dataframe->frame_id = recv_pkg.frame_id;
  dataframe->width = recv_pkg.width;
  dataframe->height = recv_pkg.height;
  dataframe->fmt = recv_pkg.fmt;

  for (int i = 0; i < CN_MAX_PLANES; i++) {
    dataframe->stride[i] = recv_pkg.stride[i];
  }

  dataframe->mlu_mem_handle = recv_pkg.mlu_mem_handle;
  // TODO: support different device ctx // NOLINT
  if (dev_ctx_.dev_type == DevContext::INVALID) {
    dataframe->ctx.dev_type = recv_pkg.ctx.dev_type;
    dataframe->ctx.dev_id = recv_pkg.ctx.dev_id;
    dataframe->ctx.ddr_channel = recv_pkg.ctx.ddr_channel;
  } else {
    dataframe->ctx.dev_type = dev_ctx_.dev_type;
    dataframe->ctx.dev_id = dev_ctx_.dev_id;
    dataframe->ctx.ddr_channel = data->GetStreamIndex() % 4;
  }

  // sync shared memory for frame data
  if (!data->IsEos()) {
    std::lock_guard<std::mutex> lock(mem_map_mutex_);
    dataframe->MmapSharedMem(memmap_type_, data->stream_id);
  }

  data->datas[CNDataFramePtrKey] = dataframe;
  return;
}

}  //  namespace cnstream
