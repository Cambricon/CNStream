/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include <gflags/gflags.h>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "data_source.hpp"
#include "displayer.hpp"
#include "util.hpp"
#include "cnstream_logging.hpp"

#include "profiler/pipeline_profiler.hpp"
#include "profiler/profile.hpp"
#include "profiler/trace_serialize_helper.hpp"
#include "gtest/gtest.h"
#include "pycnservice.hpp"
#include "pipeline_handler.hpp"
#include "cnstype.h"

namespace cnstream {

TEST(TESTWEB, TESTPYCNSERVICE) {
  std::shared_ptr<PyCNService> cnservice = std::make_shared<PyCNService>();
  CNServiceInfo service_info = {false, true, 30, 100, 1080, 620};
  cnservice->InitService(service_info);
  std::string input_filename = GetExePath() + "../web_visualize/src/webui/static/data/cars.mp4";
  std::string config_json = GetExePath() + "../web_visualize/src/webui/static/json/resnet50.json";
  cnservice->Start(input_filename, config_json);
  cnservice->WaitStop();
}

}  // namespace cnstream
