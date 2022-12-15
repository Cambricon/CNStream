/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include <memory>
#include <utility>
#include <vector>

#include "circular_buffer.hpp"
#include "profiler/pipeline_tracer.hpp"
#include "profiler/trace.hpp"

namespace cnstream {

PipelineTracer::PipelineTracer(size_t capacity) : buffer_(new CircularBuffer<TraceEvent>(capacity)) {}

PipelineTracer::~PipelineTracer() { delete buffer_; }

void PipelineTracer::RecordEvent(const TraceEvent& event) { buffer_->push_back(event); }

void PipelineTracer::RecordEvent(TraceEvent&& event) { buffer_->push_back(std::forward<TraceEvent>(event)); }

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
