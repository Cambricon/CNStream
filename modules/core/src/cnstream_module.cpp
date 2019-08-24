#include "cnstream_module.hpp"
#include "cnstream_eventbus.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {

std::atomic<unsigned int> Module::module_id_{0};

bool Module::PostEvent(EventType type, const std::string &msg) const {
  Event event;
  event.type = type;
  event.message = msg;
  event.module = this;
  if (container_) {
    return container_->GetEventBus()->PostEvent(event);
  } else {
    LOG(WARNING) << "module's container is not set";
    return false;
  }
}

}  // namespace cnstream
