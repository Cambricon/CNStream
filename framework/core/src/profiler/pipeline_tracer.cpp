#include <memory>
#include <utility>
#include <vector>

#include "circular_buffer.hpp"
#include "profiler/pipeline_tracer.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

PipelineTracer::PipelineTracer(size_t capacity)
    : buffer_(new CircularBuffer<TraceEvent>(capacity)) {}

PipelineTracer::~PipelineTracer() {
  delete buffer_;
}

void PipelineTracer::RecordEvent(const TraceEvent& event) {
  buffer_->push_back(event);
}

void PipelineTracer::RecordEvent(TraceEvent&& event) {
  buffer_->push_back(std::forward<TraceEvent>(event));
}

PipelineTrace PipelineTracer::GetTrace(const Time& start, const Time& end) const {
  if (end <= start) return {};

  PipelineTrace trace;

  auto buffer_end = buffer_->end();
  for (auto it = buffer_->begin(); it < buffer_end; ++it) {
    const TraceEvent& event = *it;
    if (event.time > start && event.time <= end) {
      if (event.level == TraceEvent::Level::PIPELINE) {
        trace.process_traces[event.process_name].emplace_back(TraceElem(event));
      } else if (event.level == TraceEvent::Level::MODULE) {
        trace.module_traces[event.module_name][event.process_name].emplace_back(TraceElem(event));
      }
    }
  }

  return trace;
}

}  // namespace cnstream
