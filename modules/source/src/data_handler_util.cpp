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
#include "data_handler_util.hpp"
#include <memory>
#include <utility>

#include "private/cnstream_allocator.hpp"

namespace cnstream {

// #define DEBUG_DUMP_IMAGE 1

int SourceRender::Process(std::shared_ptr<CNFrameInfo> frame_info, cnedk::BufSurfWrapperPtr wrapper, uint64_t frame_id,
                          const DataSourceParam &param_) {
  CNDataFramePtr dataframe = frame_info->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
  if (!dataframe) return -1;

  // send info & deleter to downstream
  dataframe->buf_surf = std::move(wrapper);
  dataframe->frame_id = frame_id;

#ifdef DEBUG_DUMP_IMAGE
  static bool flag = false;
  if (!flag) {
    imwrite("test_mlu.jpg", dataframe->ImageBGR());
    flag = true;
  }
#endif
  return 0;
}

}  // namespace cnstream
