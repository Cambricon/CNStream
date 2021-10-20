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

#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_graph.hpp"
#include "test_base.hpp"

namespace cnstream {

TEST(CoreDAGAlgorithm, AddVertex) {
  DAGAlgorithm dag;
  dag.Reserve(3);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(i, dag.AddVertex());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(dag.GetIndegree(i), 0);
    EXPECT_EQ(dag.GetOutdegree(i), 0);
  }
}

TEST(CoreDAGAlgorithm, AddEdge) {
  DAGAlgorithm dag;
  for (int i = 0; i < 3; ++i)
    dag.AddVertex();
  // case1: add success
  EXPECT_TRUE(dag.AddEdge(1, 2));
  // case2: vertex out of range
  EXPECT_FALSE(dag.AddEdge(0, 3));
  EXPECT_FALSE(dag.AddEdge(5, 1));
  // case3: add the same edge twice
  EXPECT_FALSE(dag.AddEdge(1, 2));
}

TEST(CoreDAGAlgorithm, GetIndegree) {
  DAGAlgorithm dag;
  for (int i = 0; i < 3; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(0, 2));
  ASSERT_TRUE(dag.AddEdge(1, 2));
  // case1: success
  EXPECT_EQ(2, dag.GetIndegree(2));
  // case2: vertex out of range
  EXPECT_EQ(-1, dag.GetIndegree(3));
}

TEST(CoreDAGAlgorithm, GetOutdegree) {
  DAGAlgorithm dag;
  for (int i = 0; i < 3; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(1, 2));
  ASSERT_TRUE(dag.AddEdge(1, 0));
  // case1: success
  EXPECT_EQ(2, dag.GetOutdegree(1));
  // case2: vertex out of range
  EXPECT_EQ(-1, dag.GetOutdegree(3));
}

TEST(CoreDAGAlgorithm, GetHeads) {
  DAGAlgorithm dag;
  for (int i = 0; i < 5; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(0, 1));
  ASSERT_TRUE(dag.AddEdge(0, 2));
  ASSERT_TRUE(dag.AddEdge(2, 3));
  ASSERT_TRUE(dag.AddEdge(4, 2));
  EXPECT_EQ(dag.GetHeads().size(), 2);
  EXPECT_EQ(dag.GetHeads()[0], 0);
  EXPECT_EQ(dag.GetHeads()[1], 4);
}

TEST(CoreDAGAlgorithm, GetTails) {
  DAGAlgorithm dag;
  for (int i = 0; i < 5; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(0, 1));
  ASSERT_TRUE(dag.AddEdge(0, 2));
  ASSERT_TRUE(dag.AddEdge(2, 3));
  ASSERT_TRUE(dag.AddEdge(4, 2));
  EXPECT_EQ(dag.GetTails().size(), 2);
  EXPECT_EQ(dag.GetTails()[0], 1);
  EXPECT_EQ(dag.GetTails()[1], 3);
}

TEST(CoreDAGAlgorithm, TopoSort) {
  // case1: no ring
  DAGAlgorithm dag;
  for (int i = 0; i < 5; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(0, 1));
  ASSERT_TRUE(dag.AddEdge(0, 2));
  ASSERT_TRUE(dag.AddEdge(2, 3));
  ASSERT_TRUE(dag.AddEdge(4, 2));
  std::vector<int> expected_ret = {0, 1, 4, 2, 3};
  auto ret = dag.TopoSort();
  EXPECT_EQ(5, ret.first.size());
  EXPECT_EQ(0, ret.second.size());
  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(ret.first[i], expected_ret[i]);
  // case2: with ring
  dag = DAGAlgorithm();
  for (int i = 0; i < 5; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(0, 1));
  ASSERT_TRUE(dag.AddEdge(0, 2));
  ASSERT_TRUE(dag.AddEdge(2, 3));
  ASSERT_TRUE(dag.AddEdge(4, 2));
  ASSERT_TRUE(dag.AddEdge(3, 4));
  std::vector<int> expected_sorted = {0, 1};
  std::vector<int> expected_unsorted = {2, 3, 4};
  ret = dag.TopoSort();
  EXPECT_EQ(2, ret.first.size());
  EXPECT_EQ(3, ret.second.size());
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(ret.first[i], expected_sorted[i]);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(ret.second[i], expected_unsorted[i]);
}

TEST(CoreDAGAlgorithm, DFSBegin) {
  DAGAlgorithm dag;
  for (int i = 0; i < 3; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(1, 2));
  ASSERT_TRUE(dag.AddEdge(2, 0));
  EXPECT_EQ(*dag.DFSBegin(), 1);
}

TEST(CoreDAGAlgorithm, DFSBeginFrom) {
  DAGAlgorithm dag;
  for (int i = 0; i < 3; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(1, 2));
  ASSERT_TRUE(dag.AddEdge(2, 0));
  // case1: right
  EXPECT_EQ(*dag.DFSBeginFrom(2), 2);
  // case2: vertex out of range
  EXPECT_EQ(dag.DFSBeginFrom(3), dag.DFSEnd());
}

TEST(CoreDAGAlgorithm, DFSEnd) {
  DAGAlgorithm dag;
  for (int i = 0; i < 3; ++i)
    dag.AddVertex();
  EXPECT_EQ(-1, *dag.DFSEnd());
}

TEST(CoreDAGAlgorithm, DFSOrder) {
  DAGAlgorithm dag;
  for (int i = 0; i < 5; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(0, 1));
  ASSERT_TRUE(dag.AddEdge(0, 2));
  ASSERT_TRUE(dag.AddEdge(2, 3));
  ASSERT_TRUE(dag.AddEdge(4, 2));
  // based on multiple heads
  // case1: begin
  std::vector<int> expected_order = {4, 2, 3, 0, 1};
  auto iter = dag.DFSBegin();
  for (int i = 0; i < 5; ++i, ++iter)
    EXPECT_EQ(expected_order[i], *iter);
  // case2: begin from vertex
  expected_order = {0, 1, 2, 3};
  iter = dag.DFSBeginFrom(0);
  for (int i = 0; i < 4; ++i, ++iter)
    EXPECT_EQ(expected_order[i], *iter);
  expected_order = {4, 2};
  iter = dag.DFSBeginFrom(4);
  for (int i = 0; i < 2; ++i, ++iter)
    EXPECT_EQ(expected_order[i], *iter);
}

TEST(CoreDAGAlgorithm, DFSIterNE) {
  DAGAlgorithm dag;
  for (int i = 0; i < 5; ++i)
    dag.AddVertex();
  ASSERT_TRUE(dag.AddEdge(0, 1));
  ASSERT_TRUE(dag.AddEdge(0, 2));
  ASSERT_TRUE(dag.AddEdge(2, 3));
  ASSERT_TRUE(dag.AddEdge(4, 2));
  // case: multiple heads, stack size not the same, stack top is the same
  auto iter1 = dag.DFSBegin();
  auto iter2 = dag.DFSBeginFrom(4);
  EXPECT_NE(iter1, iter2);
}

TEST(CoreCNGraph, InitNormalSimpleGraph) {
  // no subgraph, no ring
  /**
   * two source
   *       0   7
   *      / \ /
   *     1   2
   *    /   / \
   *   3   4   5
   *    \     /
   *     \   /
   *       6
   **/
  CNGraphConfig graph_config;
  graph_config.name = "test_graph";
  CNModuleConfig config0;
  config0.name = "0";
  config0.next = {"1", "2"};
  graph_config.module_configs.push_back(config0);
  CNModuleConfig config1;
  config1.name = "1";
  config1.next = {"3"};
  graph_config.module_configs.push_back(config1);
  CNModuleConfig config2;
  config2.name = "2";
  config2.next = {"4", "5"};
  graph_config.module_configs.push_back(config2);
  CNModuleConfig config3;
  config3.name = "3";
  config3.next = {"6"};
  graph_config.module_configs.push_back(config3);
  CNModuleConfig config4;
  config4.name = "4";
  config4.next = {};
  graph_config.module_configs.push_back(config4);
  CNModuleConfig config5;
  config5.name = "5";
  config5.next = {"6"};
  graph_config.module_configs.push_back(config5);
  CNModuleConfig config6;
  config6.name = "6";
  config6.next = {};
  graph_config.module_configs.push_back(config6);
  CNModuleConfig config7;
  config7.name = "7";
  config7.next = {"2"};
  graph_config.module_configs.push_back(config7);
  CNGraph<int> graph(graph_config);
  EXPECT_TRUE(graph.Init());
  // check heads
  std::vector<std::string> expected_head_names = {"0", "7"};
  EXPECT_EQ(2, graph.GetHeads().size());
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(expected_head_names[i], graph.GetHeads()[i]->GetName());
  // check tails
  std::vector<std::string> expected_tail_names = {"4", "6"};
  EXPECT_EQ(2, graph.GetTails().size());
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(expected_tail_names[i], graph.GetTails()[i]->GetName());
  // check dfs order
  std::vector<std::string> expected_dfs_order = {"7", "2", "4", "5", "6", "0", "1", "3"};
  auto iter = graph.DFSBegin();
  for (size_t i = 0; i < expected_dfs_order.size(); ++i, ++iter) {
    EXPECT_NE(iter, graph.DFSEnd());
    EXPECT_EQ(iter->GetName(), expected_dfs_order[i]);
  }
}

TEST(CoreCNGraph, InitGraphWithRing) {
  // no subgraph, has ring
  /**
   *       0   7
   *      / \ /|\    ring: 7->2->5->7
   *     1   2 |
   *    /   / \|
   *   3   4   5
   *    \     /
   *     \   /
   *       6
   **/
  CNGraphConfig graph_config;
  graph_config.name = "test_graph";
  CNModuleConfig config0;
  config0.name = "0";
  config0.next = {"1", "2"};
  graph_config.module_configs.push_back(config0);
  CNModuleConfig config1;
  config1.name = "1";
  config1.next = {"3"};
  graph_config.module_configs.push_back(config1);
  CNModuleConfig config2;
  config2.name = "2";
  config2.next = {"4", "5"};
  graph_config.module_configs.push_back(config2);
  CNModuleConfig config3;
  config3.name = "3";
  config3.next = {"6"};
  graph_config.module_configs.push_back(config3);
  CNModuleConfig config4;
  config4.name = "4";
  config4.next = {};
  graph_config.module_configs.push_back(config4);
  CNModuleConfig config5;
  config5.name = "5";
  config5.next = {"6", "7"};
  graph_config.module_configs.push_back(config5);
  CNModuleConfig config6;
  config6.name = "6";
  config6.next = {};
  graph_config.module_configs.push_back(config6);
  CNModuleConfig config7;
  config7.name = "7";
  config7.next = {"2"};
  graph_config.module_configs.push_back(config7);
  CNGraph<int> graph(graph_config);
  EXPECT_FALSE(graph.Init());
}

TEST(CoreCNGraph, InitWithSubgraph) {
  /**
   * // 1 is subgraph1, the same with parent graph, node named 1 is not subgraph, 2 is the same with subgraph2
   * // 2 is subgraph2: 0->1, 0->2, 1->3
   *       0
   *      / \ 
   *     1   2
   *    /
   *   3
   **/
  auto subgraph1_config_file = CreateTempFile("subgraph1");
  auto subgraph2_config_file = CreateTempFile("subgraph2");
  std::ofstream ofs_subgraph1(subgraph1_config_file.second);
  ofs_subgraph1 << "{\n"
    "\"0\" : {\n"
    "  \"class_name\" : \"test\",\n"
    "  \"next_modules\" : [\"1\", \"subgraph:2\"]\n"
    "},\n"
    "\"1\" : {\n"
    "  \"class_name\" : \"test\",\n"
    "  \"next_modules\" : [\"3\"]\n"
    "},\n"
    "\"subgraph:2\" : {\n"
    "  \"config_path\" : \"" + subgraph2_config_file.second + "\"\n" +
    "},\n"
    "\"3\" : {\n"
    "  \"class_name\" : \"test\"\n"
    "}\n"
  "}\n";
  ofs_subgraph1.close();
  std::ofstream ofs_subgraph2(subgraph2_config_file.second);
  ofs_subgraph2 << "{\n"
    "\"0\" : {\n"
    "  \"class_name\" : \"test\",\n"
    "  \"next_modules\" : [\"1\", \"2\"]\n"
    "},\n"
    "\"1\" : {\n"
    "  \"class_name\" : \"test\",\n"
    "  \"next_modules\" : [\"3\"]\n"
    "},\n"
    "\"2\" : {\n"
    "  \"class_name\" : \"test\"\n"
    "},\n"
    "\"3\" : {\n"
    "  \"class_name\" : \"test\"\n"
    "}\n"
  "}\n";
  ofs_subgraph2.close();
  CNGraphConfig graph_config;
  graph_config.name = "test_graph";
  CNModuleConfig config0;
  config0.name = "0";
  config0.next = {"subgraph:1", "subgraph:2"};
  graph_config.module_configs.push_back(config0);
  CNSubgraphConfig config1;
  config1.name = "subgraph:1";
  config1.config_path = subgraph1_config_file.second;
  config1.next = {"3"};
  graph_config.subgraph_configs.push_back(config1);
  CNSubgraphConfig config2;
  config2.name = "subgraph:2";
  config2.config_path = subgraph2_config_file.second;
  graph_config.subgraph_configs.push_back(config2);
  CNModuleConfig config3;
  config3.name = "3";
  graph_config.module_configs.push_back(config3);
  CNGraph<int> graph(graph_config);
  EXPECT_TRUE(graph.Init());
  // check heads
  std::vector<std::string> expected_head_names = {"test_graph/0"};
  EXPECT_EQ(1, graph.GetHeads().size());
  for (int i = 0; i < 1; ++i)
    EXPECT_EQ(expected_head_names[i], graph.GetHeads()[i]->GetFullName());
  // check tails
  std::vector<std::string> expected_tail_names = {"test_graph/3", "test_graph/2/2", "test_graph/2/3"};
  EXPECT_EQ(3, graph.GetTails().size());
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(expected_tail_names[i], graph.GetTails()[i]->GetFullName());
  // check dfs order
  std::vector<std::string> expected_dfs_order = {"test_graph/0", "test_graph/1/0", "test_graph/1/1",
      "test_graph/1/3", "test_graph/1/2/0", "test_graph/1/2/1", "test_graph/1/2/3", "test_graph/1/2/2",
      "test_graph/3", "test_graph/2/0", "test_graph/2/1", "test_graph/2/3", "test_graph/2/2"};
  auto iter = graph.DFSBegin();
  for (size_t i = 0; i < expected_dfs_order.size(); ++i, ++iter) {
    EXPECT_NE(iter, graph.DFSEnd());
    EXPECT_EQ(iter->GetFullName(), expected_dfs_order[i]);
  }

  // check GetNodeByName, no subgraph prefix, the first node named 2 by dfs order
  EXPECT_EQ(graph.GetNodeByName("2")->GetFullName(), "test_graph/1/2/2");
  EXPECT_EQ(graph.GetNodeByName("6").get(), nullptr);
  // check GetNodeByName, with subgraph prefix
  EXPECT_EQ(graph.GetNodeByName("test_graph/2/1")->GetFullName(), "test_graph/2/1");
  EXPECT_EQ(graph.GetNodeByName("test_graph/2/5").get(), nullptr);
  // check GetNodeByName, with subgraph prefix, but miss subgraph
  EXPECT_EQ(graph.GetNodeByName("test_graph/7/0").get(), nullptr);

  unlink(subgraph1_config_file.second.c_str());
  close(subgraph1_config_file.first);
  unlink(subgraph2_config_file.second.c_str());
  close(subgraph2_config_file.first);
}

TEST(CoreCNGraph, InitWithSubgraphAnalysisLoopConfig) {
  auto subgraph1_config_file = CreateTempFile("subgraph1");
  std::ofstream ofs_subgraph1(subgraph1_config_file.second);
  ofs_subgraph1 << "{\n"
    "\"0\" : {\n"
    "  \"class_name\" : \"test\",\n"
    "  \"next_modules\" : [\"1\", \"subgraph:2\"]\n"
    "},\n"
    "\"1\" : {\n"
    "  \"class_name\" : \"test\",\n"
    "  \"next_modules\" : [\"3\"]\n"
    "},\n"
    "\"subgraph:2\" : {\n"
    "  \"config_path\" : \"" + subgraph1_config_file.second + "\"\n" +  // ring
    "},\n"
    "\"3\" : {\n"
    "  \"class_name\" : \"test\"\n"
    "}\n"
  "}\n";
  ofs_subgraph1.close();
  CNGraphConfig graph_config;
  graph_config.name = "test_graph";
  CNSubgraphConfig config0;
  config0.name = "subgraph:0";
  config0.config_path = subgraph1_config_file.second;
  graph_config.subgraph_configs.push_back(config0);
  CNGraph<int> graph;
  EXPECT_FALSE(graph.Init(graph_config));
  unlink(subgraph1_config_file.second.c_str());
  close(subgraph1_config_file.first);
}

TEST(CoreCNGraph, SubgraphParseFailed) {
  CNGraphConfig graph_config;
  graph_config.name = "test_graph";
  CNSubgraphConfig config0;
  config0.name = "subgraph:0";
  config0.config_path = "wrong_path";
  graph_config.subgraph_configs.push_back(config0);
  CNGraph<int> graph;
  EXPECT_FALSE(graph.Init(graph_config));
}

TEST(CoreCNGraph, ModuleNodeNameInvalid) {
  CNGraphConfig graph_config;
  graph_config.name = "test_graph";
  CNModuleConfig config0;
  config0.name = "0:0";  // : is not allow
  graph_config.module_configs.push_back(config0);
  CNGraph<int> graph;
  EXPECT_FALSE(graph.Init(graph_config));
  graph_config.module_configs[0].name = "0/0";  // / is not allow
  EXPECT_FALSE(graph.Init(graph_config));
}

TEST(CoreCNGraph, SubgraphNodeNameInvalid) {
  CNGraphConfig graph_config;
  graph_config.name = "test_graph";
  CNSubgraphConfig config0;
  config0.name = "subgraph:0:0";  // : is not allow
  graph_config.subgraph_configs.push_back(config0);
  CNGraph<int> graph;
  EXPECT_FALSE(graph.Init(graph_config));
  graph_config.subgraph_configs[0].name = "subgraph:0/0";  // / is not allow
  EXPECT_FALSE(graph.Init(graph_config));
}

TEST(CoreCNGraph, WrongEdge) {
  {
    CNGraphConfig graph_config;
    graph_config.name = "test_graph";
    // module->module
    CNModuleConfig config0;
    config0.name = "0";
    config0.next = {"1"};
    graph_config.module_configs.push_back(config0);
    CNGraph<int> graph;
    EXPECT_FALSE(graph.Init(graph_config));
    // module->subgraph
    graph_config.module_configs[0].next = {"subgraph:1"};
    EXPECT_FALSE(graph.Init(graph_config));
  }
  {
    CNGraphConfig graph_config;
    graph_config.name = "test_graph";
    auto subgraph_config_file = CreateTempFile("subgraph_empty");
    std::ofstream ofs(subgraph_config_file.second);
    ofs << "{}";
    ofs.close();
    // subgraph->module
    CNSubgraphConfig config0;
    config0.name = "subgraph:0";
    config0.next = {"1"};
    config0.config_path = subgraph_config_file.second;
    graph_config.subgraph_configs.push_back(config0);
    CNGraph<int> graph;
    EXPECT_FALSE(graph.Init(graph_config));
    // subgraph->subgraph
    graph_config.subgraph_configs[0].next = {"subgraph:1"};
    EXPECT_FALSE(graph.Init(graph_config));
    unlink(subgraph_config_file.second.c_str());
    close(subgraph_config_file.first);
  }
}

}  // namespace cnstream
