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
#ifndef LIBSTREAM_INCLUDE_CNINFER_MLU_CONTEXT_H_
#define LIBSTREAM_INCLUDE_CNINFER_MLU_CONTEXT_H_

#include "cnbase/streamlibs_error.h"

namespace libstream {

STREAMLIBS_REGISTER_EXCEPTION(MluContext);

class MluContext {
 public:
  static const int s_channel_num = 4;
  MluContext() = default;
  explicit MluContext(int dev_id);
  MluContext(int dev_id, int channel_id);
  int dev_id() const;
  void set_dev_id(int id);
  int channel_id() const;
  void set_channel_id(int id);

  void ConfigureForThisThread() const;

 private:
  int dev_id_ = 0;
  int channel_id_ = -1;  // -1 means batch_size=1
};  // class MluContext

}  // namespace libstream

#endif  // LIBSTREAM_INCLUDE_CNINFER_MLU_CONTEXT_H_
