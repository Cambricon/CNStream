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
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "osd_handler.hpp"

using DrawInfo = cnstream::OsdHandler::DrawInfo;

class OsdHandlerVS : public cnstream::OsdHandler {
 public:
  ~OsdHandlerVS();
  int GetDrawInfo(const CNObjsVec &objects, const std::vector<std::string> &labels,
                  std::vector<DrawInfo> *info) override;

 private:
  struct ObjectInfo {
    int life_time;
    bool appeared = false;
    std::vector<std::string> attributes;
  };
  std::unordered_map<std::string, ObjectInfo> objects_pool_;
  int life_time_ = 30;
  DECLARE_REFLEX_OBJECT_EX(OsdHandlerVS, cnstream::OsdHandler);
};

IMPLEMENT_REFLEX_OBJECT_EX(OsdHandlerVS, cnstream::OsdHandler);

OsdHandlerVS::~OsdHandlerVS() { objects_pool_.clear(); }

int OsdHandlerVS::GetDrawInfo(const CNObjsVec &objects, const std::vector<std::string> &labels,
                              std::vector<DrawInfo> *info) {
  for (auto &obj : objects) {
    DrawInfo draw_info;
    draw_info.bbox = GetFullFovBbox(obj.get());

    // Label
    if (!obj->id.empty()) {
      int id = std::stoi(obj->id);
      if (labels.size() <= static_cast<size_t>(id)) {
        draw_info.label_id = -1;
        draw_info.basic_info = "NoLabel";
      } else {
        draw_info.label_id = id;
        draw_info.basic_info = labels[id];
      }
    } else {
      draw_info.label_id = -1;
      draw_info.basic_info = "NoLabel";
    }
    // Score
    std::stringstream ss;
    ss << setiosflags(std::ios::fixed) << std::setprecision(2) << obj->score;
    draw_info.basic_info += " " + ss.str();

    // Track Id
    if (!obj->track_id.empty() && std::stoi(obj->track_id) >= 0) draw_info.basic_info += " track_id:" + obj->track_id;
    std::vector<std::string> attributes;

    if (obj->GetExtraAttribute("Category") == "Plate") {
      std::string plateNumber = obj->GetExtraAttribute("PlateNumber");
      // LOGI(DEMO) << "PlateNumber ----" << plateNumber;
      // for CE3226, chinese characters not supported yet, remove them at the moment.
      // FIXME
      std::string non_chinese;
      for (size_t i = 0; i < plateNumber.length(); i++) {
        int cnt = 0;
        unsigned char c = plateNumber[i];
        while (c & (0x80 >> cnt)) ++cnt;
        if (cnt) {
          i += cnt - 1;
          continue;
        }
        non_chinese.push_back(c);
      }
      draw_info.basic_info = non_chinese;
      // for MLUxxx
      // draw_info.basic_info = plateNumber;
      info->push_back(draw_info);
      continue;
    }

    if (obj->GetExtraAttribute("SkipObject") == "") {
      // Attributes
      std::string obj_attr;
      std::vector<std::string> range;
      std::string category = obj->GetExtraAttribute("Category");
      if (category == "Pedestrain") {
        // Gender
        obj_attr = obj->GetExtraAttribute("Sex");
        if (!obj_attr.empty()) {
          attributes.push_back(std::atof(obj_attr.c_str()) > 0.5 ? "Female" : "Male");
        }

        // Age
        float attrs_score[3];
        float max_attr_score = 0;
        int max_attr_index = 0;
        range = {"age_<16", "age_16~60", "age_60+"};
        for (size_t i = 0; i < 3; ++i) {
          obj_attr = obj->GetExtraAttribute(range[i]);
          attrs_score[i] = obj_attr.empty() ? 0 : std::atof(obj_attr.c_str());
          if (attrs_score[i] > max_attr_score) {
            max_attr_score = attrs_score[i];
            max_attr_index = i;
          }
        }
        if (max_attr_score != 0) {
          attributes.push_back(range[max_attr_index]);
        }

        // Orient
        attrs_score[0] = attrs_score[1] = attrs_score[2] = 0;
        max_attr_score = 0;
        max_attr_index = 0;
        range = {"orient_front", "orient_side", "orient_back"};
        for (size_t i = 0; i < 3; ++i) {
          obj_attr = obj->GetExtraAttribute(range[i]);
          attrs_score[0] = obj_attr.empty() ? 0 : std::atof(obj_attr.c_str());
          if (attrs_score[i] > max_attr_score) {
            max_attr_score = attrs_score[i];
            max_attr_index = i;
          }
        }
        if (max_attr_score != 0) {
          attributes.push_back(range[max_attr_index]);
        }

        range = {"hat", "glasses", "handbag", "knapsack", "shoulderbag", "long_sleeve"};
        for (const auto &item : range) {
          obj_attr = obj->GetExtraAttribute(item);
          if (!obj_attr.empty() && std::atof(obj_attr.c_str()) > 0.5) {
            attributes.push_back(item);
          }
        }

        // range = {"clothesStyle", "clothesColor", "trousersStyle", "shoesStyle", "trousers_length",
        //          "upper_length", "age", "hairstyle", "accessory", "gender", "trousersColor"};
        range = {"age", "hairstyle", "gender"};
        for (const auto &item : range) {
          obj_attr = obj->GetExtraAttribute(item);
          if (obj_attr != "") {
            auto val_score = cnstream::StringSplit(obj_attr, ':');
            if (std::atof(val_score[1].c_str()) > 0.5) {
              attributes.push_back(item + ": " + val_score[0]);
            }
          }
        }
      } else if (category == "Vehicle") {
        range = {"Brand", "Series", "Color", "Type", "Side"};
        for (const auto &item : range) {
          obj_attr = obj->GetExtraAttribute(item);
          if (obj_attr != "") {
            auto val_score = cnstream::StringSplit(obj_attr, ':');
            if (std::atof(val_score[1].c_str()) > 0.5) {
              attributes.push_back(item + ": " + val_score[0]);
            }
          }
        }
        range = {"PlateNumber", "PlateType"};
        for (const auto &item : range) {
          auto obj_attr = obj->GetExtraAttribute(item);
          if (obj_attr != "") {
            attributes.push_back(item + ": " + obj_attr);
          }
        }
      }
    }

    if (objects_pool_.find(obj->track_id) != objects_pool_.end()) {
      auto &obj_info = objects_pool_[obj->track_id];
      if (attributes.size() < obj_info.attributes.size()) {
        attributes = obj_info.attributes;
      } else {
        obj_info.attributes = attributes;
      }
      obj_info.life_time = life_time_;
      obj_info.appeared = true;
    } else {
      ObjectInfo obj_info;
      obj_info.life_time = life_time_;
      obj_info.appeared = true;
      obj_info.attributes = attributes;
      objects_pool_.insert(std::make_pair(obj->track_id, obj_info));
    }
    draw_info.attributes = attributes;
    info->push_back(draw_info);
  }

  // UpdateFrame
  for (auto it = objects_pool_.begin(); it != objects_pool_.end();) {
    if (it->second.appeared) {
      it->second.appeared = false;
    } else {
      it->second.life_time--;
    }
    if (it->second.life_time <= 0) {
      it = objects_pool_.erase(it);
    } else {
      ++it;
    }
  }
  return 0;
}
