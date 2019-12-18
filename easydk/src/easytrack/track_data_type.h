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

#ifndef EASYTRACK_TRACK_DATA_TYPE_H_
#define EASYTRACK_TRACK_DATA_TYPE_H_

#include <utility>
#include <vector>

#include "easytrack/easy_track.h"

namespace edk {

struct Rect {
  float xmin;
  float ymin;
  float xmax;
  float ymax;
};

inline Rect BoundingBox2Rect(const BoundingBox &bbox) {
  Rect rect;
  rect.xmin = bbox.x;
  rect.ymin = bbox.y;
  rect.xmax = bbox.x + bbox.width;
  rect.ymax = bbox.y + bbox.height;
  return rect;
}

inline BoundingBox Rect2BoundingBox(const Rect &rect) {
  BoundingBox bbox;
  bbox.x = rect.xmin;
  bbox.y = rect.ymin;
  bbox.width = rect.xmax - bbox.x;
  bbox.height = rect.ymax - bbox.y;
  return bbox;
}

enum class TrackState { TENTATIVE, CONFIRMED, DELETED };

using MatchData = std::pair<int, int>;

using CostMatrix = std::vector<std::vector<float>>;

struct MatchResult {
  std::vector<MatchData> matches;
  std::vector<int> unmatched_tracks;
  std::vector<int> unmatched_detections;
};

}  // namespace edk

#endif  // EASYTRACK_TRACK_DATA_TYPE_H_
