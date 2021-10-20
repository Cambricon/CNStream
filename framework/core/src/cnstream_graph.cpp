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

#include "cnstream_graph.hpp"

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_logging.hpp"

namespace cnstream {

std::vector<int> DAGAlgorithm::GetHeads() const {
  std::vector<int> heads;
  for (int vertex_idx = 0; vertex_idx < static_cast<int>(edges_.size());
      ++vertex_idx) {
    if (0 == GetIndegree(vertex_idx)) heads.push_back(vertex_idx);
  }
  return heads;
}

std::vector<int> DAGAlgorithm::GetTails() const {
  std::vector<int> tails;
  for (int vertex_idx = 0; vertex_idx < static_cast<int>(edges_.size());
      ++vertex_idx) {
    if (0 == GetOutdegree(vertex_idx)) tails.push_back(vertex_idx);
  }
  return tails;
}

std::pair<std::vector<int>, std::vector<int>>
DAGAlgorithm::TopoSort() const {
  std::vector<int> sorted_vertices, unsorted_vertices;
  auto indegrees = indegrees_;
  bool has_vertices_removed = true;
  // remove vertices with indegree equal to 0 and update indegrees
  while (has_vertices_removed) {
    has_vertices_removed = false;
    for (int vertex_idx = 0; vertex_idx < static_cast<int>(indegrees.size());
        ++vertex_idx) {
      if (0 == indegrees[vertex_idx]) {
        sorted_vertices.push_back(vertex_idx);
        // remove vertex
        for (auto end_vertex : edges_[vertex_idx]) {
          indegrees[end_vertex]--;
        }
        indegrees[vertex_idx] = -1;  // -1 means removed.
        has_vertices_removed = true;
      }
    }
  }
  // check unremoved
  for (int vertex_idx = 0; vertex_idx < static_cast<int>(indegrees.size());
      ++vertex_idx) {
    if (-1 != indegrees[vertex_idx]) unsorted_vertices.push_back(vertex_idx);
  }
  return std::make_pair(std::move(sorted_vertices), std::move(unsorted_vertices));
}

DAGAlgorithm::DFSIterator DAGAlgorithm::DFSBegin() const {
  DFSIterator iter(this);
  iter.visit_.resize(edges_.size(), false);
  auto heads = GetHeads();
  for (auto head : heads)
    iter.vertex_stack_.push(head);
  if (!iter.vertex_stack_.empty()) iter.visit_[iter.vertex_stack_.top()] = true;
  return iter;
}

DAGAlgorithm::DFSIterator DAGAlgorithm::DFSBeginFrom(int vertex) const {
  if (vertex >= static_cast<int>(edges_.size()) || vertex < 0) return DFSEnd();
  DFSIterator iter(this);
  iter.visit_.resize(edges_.size(), false);
  iter.vertex_stack_.push(vertex);
  iter.visit_[vertex] = true;
  return iter;
}

DAGAlgorithm::DFSIterator&
DAGAlgorithm::DFSIterator::operator++() {
  while (!vertex_stack_.empty()) {
    auto cur_vertex = vertex_stack_.top();
    if (!visit_[cur_vertex]) break;  // for multiple heads
    const auto& edges = dag_->edges_[cur_vertex];
    auto edge_iter = edges.begin();
    for (; edge_iter != edges.end(); ++edge_iter) {
      if (!visit_[*edge_iter]) break;
    }
    if (edge_iter == edges.end()) {
      vertex_stack_.pop();
    } else {
      vertex_stack_.push(*edge_iter);
      break;
    }
  }
  if (!vertex_stack_.empty())
    visit_[vertex_stack_.top()] = true;
  return *this;
}

bool DAGAlgorithm::DFSIterator::operator==(const DAGAlgorithm::DFSIterator& other) const {
  return dag_ == other.dag_ && vertex_stack_.size() == other.vertex_stack_.size() &&
         (vertex_stack_.size() ? vertex_stack_.top() == other.vertex_stack_.top() : true);
}

}  // namespace cnstream
