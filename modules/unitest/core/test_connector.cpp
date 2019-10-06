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
#include <ctime>
#include <memory>
#include <vector>

#include "cnstream_frame.hpp"
#include "connector.hpp"
#include "conveyor.hpp"

namespace cnstream {

uint32_t seed = (uint32_t)time(0);

TEST(CoreConnector, SetGetParams) {
  size_t conveyor_count = rand_r(&seed) % 100;
  size_t conveyor_capacity = rand_r(&seed) % 100;
  Connector connector(conveyor_count, conveyor_capacity);
  EXPECT_EQ(conveyor_count, connector.GetConveyorCount());
  EXPECT_EQ(conveyor_capacity, connector.GetConveyorCapacity());
}

TEST(CoreConnector, GetConveyor) {
  size_t conveyor_count = 10;
  Connector connector(conveyor_count);
  int idx = rand_r(&seed) % conveyor_count;
  EXPECT_NE(nullptr, connector.GetConveyor(idx));
  EXPECT_DEATH(connector.GetConveyor(conveyor_count + 1), "") << "conveyor vector out of range not reported";
  EXPECT_DEATH(connector.GetConveyor(-1), "") << "conveyor vector out of range not reported";
}

TEST(CoreConnector, PushPopDataBuffer) {
  size_t conveyor_count = 1;
  Connector connector(conveyor_count);
  CNFrameInfoPtr data = CNFrameInfo::Create("stream_id_0");
  connector.PushDataBufferToConveyor(0, data);
  CNFrameInfoPtr out_data = connector.PopDataBufferFromConveyor(0);
  EXPECT_EQ(data.get(), out_data.get());
}

TEST(CoreConnector, StartStop) {
  size_t conveyor_count = 10;
  Connector connector(conveyor_count);
  connector.Start();
  EXPECT_FALSE(connector.IsStopped());
  connector.Stop();
  EXPECT_TRUE(connector.IsStopped());
}

}  // namespace cnstream
