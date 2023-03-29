#ifndef CNSTREAM_CNRT_WRAP_HPP_
#define CNSTREAM_CNRT_WRAP_HPP_

#include <string>

#include "cnrt.h"

// cnrt changed api name at v5.0.0 and v6.0.0
namespace cnrt {

#if CNRT_MAJOR_VERSION < 5
class CnrtInit {
 public:
  CnrtInit() { cnrtInit(0); }
  ~CnrtInit() { cnrtDestroy(); }
};
static CnrtInit cnrt_init_;

// cnrtQueue_t
inline cnrtRet_t QueueCreate(cnrtQueue_t *pqueue) { return cnrtCreateQueue(pqueue); }
inline cnrtRet_t QueueDestroy(cnrtQueue_t queue) { return cnrtDestroyQueue(queue); }
inline cnrtRet_t QueueSync(cnrtQueue_t queue) { return cnrtSyncQueue(queue); }
inline cnrtRet_t BindDevice(int32_t device_id) {
  cnrtDev_t dev;
  cnrtRet_t ret = CNRT_RET_SUCCESS;
  ret = cnrtGetDeviceHandle(&dev, device_id);
  if (ret != CNRT_RET_SUCCESS) {
    return ret;
  }
  return cnrtSetCurrentDevice(dev);
}
inline bool CheckDeviceId(int32_t device_id) {
  cnrtDev_t dev;
  return CNRT_RET_SUCCESS == cnrtGetDeviceHandle(&dev, device_id);
}
inline std::string GetDeviceName(int32_t device_id) {
  cnrtDeviceInfo_t dev_info;
  cnrtRet_t cnrt_ret = cnrtGetDeviceInfo(&dev_info, device_id);
  if (CNRT_RET_SUCCESS != cnrt_ret) {
    return "";
  }
  return dev_info.device_name;
}
#else
// cnrtQueue_t
inline cnrtRet_t QueueCreate(cnrtQueue_t *pqueue) { return cnrtQueueCreate(pqueue); }
inline cnrtRet_t QueueDestroy(cnrtQueue_t queue) { return cnrtQueueDestroy(queue); }
inline cnrtRet_t QueueSync(cnrtQueue_t queue) { return cnrtQueueSync(queue); }
inline cnrtRet_t BindDevice(int32_t device_id) {
  return cnrtSetDevice(device_id);
}
inline bool CheckDeviceId(int32_t device_id) {
  unsigned int count;
  cnrtGetDeviceCount(&count);
  if (device_id >= static_cast<int>(count) || device_id < 0) {
    return false;
  }
  return true;
}
inline std::string GetDeviceName(int32_t device_id) {
  cnrtDeviceProp_t dev_prop;
  cnrtRet_t cnrt_ret = cnrtGetDeviceProperties(&dev_prop, device_id);
  if (CNRT_RET_SUCCESS != cnrt_ret) {
    return "";
  }
  return dev_prop.name;
}
#endif

}  // namespace cnrt

#endif  // CNSTREAM_CNRT_WRAP_HPP_
