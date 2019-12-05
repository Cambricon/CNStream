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

#include "pose_utils.hpp"

#include <algorithm>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

namespace openpose {

float getScaleFactor(cv::Size srcSize, cv::Size scaledSize) {
  float ratio = scaledSize.height / static_cast<float>(srcSize.height);
  int scaled_width = srcSize.width * ratio;

  if (scaled_width > scaledSize.width) {
    scaled_width = scaledSize.width;
    ratio = scaledSize.width / static_cast<float>(srcSize.width);
  }

  return (1.0 / ratio);
}

cv::Mat getScaledImg(const cv::Mat &im, cv::Size scaledSize) {
  int scaledHeight = scaledSize.height;
  float ratio = scaledSize.height / static_cast<float>(im.rows);
  int scaledWidth = im.cols * ratio;

  if (scaledWidth > scaledSize.width) {
    scaledWidth = scaledSize.width;
    ratio = scaledSize.width / static_cast<float>(im.cols);
    scaledHeight = im.rows * ratio;
  }

  cv::Rect dst(0, 0, scaledWidth, scaledHeight);
  cv::Mat dstImg = cv::Mat::zeros(scaledSize.height, scaledSize.width, CV_8UC3);
  resize(im, dstImg(dst), cv::Size(scaledWidth, scaledHeight));
  return dstImg;
}

BlobData *createBlobData(int num, int channels, int height, int width) {
  BlobData *blob = new BlobData();
  blob->num = num;
  blob->width = width;
  blob->channels = channels;
  blob->height = height;
  blob->count = num * width * channels * height;
  blob->list = new float[blob->count];
  blob->capacity_count = blob->count;
  return blob;
}

void releaseBlobData(BlobData **blob) {
  if (blob) {
    BlobData *ptr = *blob;
    if (ptr) {
      if (ptr->list)
        delete ptr->list;
      delete ptr;
    }
    *blob = 0;
  }
}

void nms(BlobData *input_blob, BlobData *output_blob, float nms_threshold) {
  int src_w = input_blob->width;
  int src_h = input_blob->height;
  int src_plane_offset = src_w * src_h;
  float *src_ptr = input_blob->list;
  float *dst_ptr = output_blob->list;
  int dst_plane_offset = output_blob->width * output_blob->height;

  int max_peaks = output_blob->height - 1;

  for (int n = 0; n < input_blob->num; ++n) {
    for (int c = 0; c < input_blob->channels - 1; ++c) {
      int num_peaks = 0;
      for (int y = 1; y < src_h - 1 && num_peaks != max_peaks; ++y) {
        for (int x = 1; x < src_w - 1 && num_peaks != max_peaks; ++x) {
          float value = src_ptr[y * src_w + x];
          if (value > nms_threshold) {
            const float topLeft = src_ptr[(y - 1) * src_w + x - 1];
            const float top = src_ptr[(y - 1) * src_w + x];
            const float topRight = src_ptr[(y - 1) * src_w + x + 1];
            const float left = src_ptr[y * src_w + x - 1];
            const float right = src_ptr[y * src_w + x + 1];
            const float bottomLeft = src_ptr[(y + 1) * src_w + x - 1];
            const float bottom = src_ptr[(y + 1) * src_w + x];
            const float bottomRight = src_ptr[(y + 1) * src_w + x + 1];

            if (value > topLeft && value > top && value > topRight &&
                value > left && value > right && value > bottomLeft &&
                value > bottom && value > bottomRight) {
              float xAcc = 0;
              float yAcc = 0;
              float scoreAcc = 0;
              for (int kx = -3; kx <= 3; ++kx) {
                int ux = x + kx;
                if (ux >= 0 && ux < src_w) {
                  for (int ky = -3; ky <= 3; ++ky) {
                    int uy = y + ky;
                    if (uy >= 0 && uy < src_h) {
                      float score = src_ptr[uy * src_w + ux];
                      xAcc += ux * score;
                      yAcc += uy * score;
                      scoreAcc += score;
                    }
                  }
                }
              }

              xAcc /= scoreAcc;
              yAcc /= scoreAcc;
              scoreAcc = value;
              dst_ptr[(num_peaks + 1) * 3 + 0] = xAcc;
              dst_ptr[(num_peaks + 1) * 3 + 1] = yAcc;
              dst_ptr[(num_peaks + 1) * 3 + 2] = scoreAcc;
              num_peaks++;
            }
          }
        }
      }
      dst_ptr[0] = num_peaks;
      src_ptr += src_plane_offset;
      dst_ptr += dst_plane_offset;
    }
  }
}

float getScoreAB(const int i, const int j, const float *const candidateAPtr,
                 const float *const candidateBPtr, const float *const mapX,
                 const float *const mapY, const cv::Size &heatMapSize,
                 const float interThreshold,
                 const float interMinAboveThreshold) {
  const auto vectorAToBX = candidateBPtr[3 * j] - candidateAPtr[3 * i];
  const auto vectorAToBY = candidateBPtr[3 * j + 1] - candidateAPtr[3 * i + 1];
  const auto vectorAToBMax =
      fastMax(std::abs(vectorAToBX), std::abs(vectorAToBY));
  const auto numberPointsInLine =
      fastMax(5, fastMin(25, intRound(std::sqrt(5 * vectorAToBMax))));

  const auto vectorNorm = static_cast<float>(
      std::sqrt(vectorAToBX * vectorAToBX + vectorAToBY * vectorAToBY));
  // If the peaksPtr are coincident. Don't connect them.
  if (vectorNorm > 1e-6) {
    const auto sX = candidateAPtr[3 * i];
    const auto sY = candidateAPtr[3 * i + 1];
    const auto vectorAToBNormX = vectorAToBX / vectorNorm;
    const auto vectorAToBNormY = vectorAToBY / vectorNorm;

    auto sum = 0.;
    auto count = 0;
    const auto vectorAToBXInLine = vectorAToBX / numberPointsInLine;
    const auto vectorAToBYInLine = vectorAToBY / numberPointsInLine;
    for (auto lm = 0; lm < numberPointsInLine; lm++) {
      int mX = fastMax(0, fastMin(heatMapSize.width - 1,
                                  intRound(sX + lm * vectorAToBXInLine)));
      int mY = fastMax(0, fastMin(heatMapSize.height - 1,
                                  intRound(sY + lm * vectorAToBYInLine)));
      int idx = mY * heatMapSize.width + mX;
      const auto score =
          (vectorAToBNormX * mapX[idx] + vectorAToBNormY * mapY[idx]);
      if (score > interThreshold) {
        sum += score;
        count++;
      }
    }

    if (count / static_cast<float>(numberPointsInLine) > interMinAboveThreshold)
      return sum / count;
  }
  return 0.f;
}

std::vector<std::pair<std::vector<int>, float>>
createPeopleVector(const float *const heatMapPtr, const float *const peaksPtr,
                   const cv::Size &heatMapSize, const int maxPeaks,
                   const float interThreshold,
                   const float interMinAboveThreshold,
                   const std::vector<unsigned int> &bodyPartPairs,
                   const unsigned int numberBodyParts,
                   const unsigned int numberBodyPartPairs) {
  // std::vector<std::pair<std::vector<int>, double>> peopleVector
  // refers to:
  //     - std::vector<int>: [body parts locations, #body parts found]
  //     - double: person subset score
  std::vector<std::pair<std::vector<int>, float>> peopleVector;
  const auto &mapIdx = POSE_MAP_INDEX;
  const auto numberBodyPartsAndBkg = numberBodyParts + 1;
  const auto subsetSize = numberBodyParts + 1;
  const auto peaksOffset = 3 * (maxPeaks + 1);
  int heatMapOffset = heatMapSize.area();

  // Iterate over it PAF connection, e.g., neck-nose, neck-Lshoulder, etc.
  for (auto pairIndex = 0u; pairIndex < numberBodyPartPairs; pairIndex++) {
    const auto bodyPartA = bodyPartPairs[2 * pairIndex];
    const auto bodyPartB = bodyPartPairs[2 * pairIndex + 1];
    const auto *candidateAPtr = peaksPtr + bodyPartA * peaksOffset;
    const auto *candidateBPtr = peaksPtr + bodyPartB * peaksOffset;
    const auto numberPeaksA = intRound(candidateAPtr[0]);
    const auto numberPeaksB = intRound(candidateBPtr[0]);

    // E.g., neck-nose connection. If one of them is empty (e.g., no noses
    // detected)
    // Add the non-empty elements into the peopleVector
    if (numberPeaksA == 0 || numberPeaksB == 0) {
      // E.g., neck-nose connection. If no necks, add all noses
      // Change w.r.t. other
      if (numberPeaksA == 0) {  // numberPeaksB == 0 or no
        for (auto i = 1; i <= numberPeaksB; i++) {
          bool found = false;
          for (const auto &personVector : peopleVector) {
            const auto off =
                static_cast<int>(bodyPartB) * peaksOffset + i * 3 + 2;
            if (personVector.first[bodyPartB] == off) {
              found = true;
              break;
            }
          }
          // Add new personVector with this element
          if (!found) {
            std::vector<int> rowVector(subsetSize, 0);
            // Store the index
            rowVector[bodyPartB] = bodyPartB * peaksOffset + i * 3 + 2;
            // Last number in each row is the parts number of that person
            rowVector.back() = 1;
            const auto subsetScore = candidateBPtr[i * 3 + 2];
            // Second last number in each row is the total score
            peopleVector.emplace_back(std::make_pair(rowVector, subsetScore));
          }
        }
      } else {  // E.g., neck-nose connection. If no noses, add all necks
               // if (numberPeaksA != 0 && numberPeaksB == 0)
        // Non-MPI
        for (auto i = 1; i <= numberPeaksA; i++) {
          bool found = false;
          const auto indexA = bodyPartA;
          for (const auto &personVector : peopleVector) {
            const auto off =
                static_cast<int>(bodyPartA) * peaksOffset + i * 3 + 2;
            if (personVector.first[indexA] == off) {
              found = true;
              break;
            }
          }
          if (!found) {
            std::vector<int> rowVector(subsetSize, 0);
            // Store the index
            rowVector[bodyPartA] = bodyPartA * peaksOffset + i * 3 + 2;
            // Last number in each row is the parts number of that person
            rowVector.back() = 1;
            // Second last number in each row is the total score
            const auto subsetScore = candidateAPtr[i * 3 + 2];
            peopleVector.emplace_back(std::make_pair(rowVector, subsetScore));
          }
        }
      }
    } else {  // E.g., neck-nose connection. If necks and noses, look for
             // maximums
             // if (numberPeaksA != 0 && numberPeaksB != 0)
      // (score, indexA, indexB). Inverted order for easy std::sort
      std::vector<std::tuple<double, int, int>> allABConnections;
      // Note: Problem of this function, if no right PAF between A and B, both
      // elements are
      // discarded. However, they should be added indepently, not discarded
      if (heatMapPtr != nullptr) {
        const float *mapX =
            heatMapPtr +
            (numberBodyPartsAndBkg + mapIdx[2 * pairIndex]) * heatMapOffset;
        const float *mapY =
            heatMapPtr +
            (numberBodyPartsAndBkg + mapIdx[2 * pairIndex + 1]) * heatMapOffset;
        // E.g., neck-nose connection. For each neck
        for (auto i = 1; i <= numberPeaksA; i++) {
          // E.g., neck-nose connection. For each nose
          for (auto j = 1; j <= numberPeaksB; j++) {
            // Initial PAF
            auto scoreAB =
                getScoreAB(i, j, candidateAPtr, candidateBPtr, mapX, mapY,
                           heatMapSize, interThreshold, interMinAboveThreshold);

            // E.g., neck-nose connection. If possible PAF between neck i, nose
            // j --> add
            // parts score + connection score
            if (scoreAB > 1e-6)
              allABConnections.emplace_back(std::make_tuple(scoreAB, i, j));
          }
        }
      }

      // select the top minAB connection, assuming that each part occur only
      // once
      // sort rows in descending order based on parts + connection score
      if (!allABConnections.empty())
        std::sort(allABConnections.begin(), allABConnections.end(),
                  std::greater<std::tuple<double, int, int>>());

      std::vector<std::tuple<int, int, double>> abConnections;  // (x, y, score)
      {
        const auto minAB = fastMin(numberPeaksA, numberPeaksB);
        std::vector<int> occurA(numberPeaksA, 0);
        std::vector<int> occurB(numberPeaksB, 0);
        auto counter = 0;
        for (const auto &aBConnection : allABConnections) {
          const auto score = std::get<0>(aBConnection);
          const auto indexA = std::get<1>(aBConnection);
          const auto indexB = std::get<2>(aBConnection);
          if (!occurA[indexA - 1] && !occurB[indexB - 1]) {
            abConnections.emplace_back(std::make_tuple(
                bodyPartA * peaksOffset + indexA * 3 + 2,
                bodyPartB * peaksOffset + indexB * 3 + 2, score));
            counter++;
            if (counter == minAB)
              break;
            occurA[indexA - 1] = 1;
            occurB[indexB - 1] = 1;
          }
        }
      }

      // Cluster all the body part candidates into peopleVector based on the
      // part connection
      if (!abConnections.empty()) {
        // initialize first body part connection 15&16
        if (pairIndex == 0) {
          for (const auto &abConnection : abConnections) {
            std::vector<int> rowVector(numberBodyParts + 3, 0);
            const auto indexA = std::get<0>(abConnection);
            const auto indexB = std::get<1>(abConnection);
            const auto score = std::get<2>(abConnection);
            rowVector[bodyPartPairs[0]] = indexA;
            rowVector[bodyPartPairs[1]] = indexB;
            rowVector.back() = 2;
            // add the score of parts and the connection
            const auto personScore =
                peaksPtr[indexA] + peaksPtr[indexB] + score;
            peopleVector.emplace_back(std::make_pair(rowVector, personScore));
          }
        } else if ((numberBodyParts == 18 &&
                    (pairIndex == 17 || pairIndex == 18)) ||
                   ((numberBodyParts == 19 || (numberBodyParts == 25) ||
                     numberBodyParts == 59 || numberBodyParts == 65) &&
                    (pairIndex == 18 || pairIndex == 19))) {
          // Add ears connections (in case person is looking to opposite
          // direction
          // to camera)
          // Note: This has some issues:
          //     - It does not prevent repeating the same keypoint in different
          //     people
          //     - Assuming I have nose,eye,ear as 1 person subset, and whole
          //     arm
          //     as another one, it will not
          //       merge them both
          for (const auto &abConnection : abConnections) {
            const auto indexA = std::get<0>(abConnection);
            const auto indexB = std::get<1>(abConnection);
            for (auto &personVector : peopleVector) {
              auto &personVectorA = personVector.first[bodyPartA];
              auto &personVectorB = personVector.first[bodyPartB];
              if (personVectorA == indexA && personVectorB == 0) {
                personVectorB = indexB;
                // // This seems to harm acc 0.1% for BODY_25
                // personVector.first.back()++;
              } else if (personVectorB == indexB && personVectorA == 0) {
                personVectorA = indexA;
                // // This seems to harm acc 0.1% for BODY_25
                // personVector.first.back()++;
              }
            }
          }
        } else {
          // A is already in the peopleVector, find its connection B
          for (const auto &abConnection : abConnections) {
            const auto indexA = std::get<0>(abConnection);
            const auto indexB = std::get<1>(abConnection);
            const auto score = static_cast<float>(std::get<2>(abConnection));
            bool found = false;
            for (auto &personVector : peopleVector) {
              // Found partA in a peopleVector, add partB to same one.
              if (personVector.first[bodyPartA] == indexA) {
                personVector.first[bodyPartB] = indexB;
                personVector.first.back()++;
                personVector.second += peaksPtr[indexB] + score;
                found = true;
                break;
              }
            }
            // Not found partA in peopleVector, add new peopleVector element
            if (!found) {
              std::vector<int> rowVector(subsetSize, 0);
              rowVector[bodyPartA] = indexA;
              rowVector[bodyPartB] = indexB;
              rowVector.back() = 2;
              const auto personScore =
                  peaksPtr[indexA] + peaksPtr[indexB] + score;
              peopleVector.emplace_back(std::make_pair(rowVector, personScore));
            }
          }
        }
      }
    }
  }

  // int i = 0;
  // for (auto& personvector : peoplevector) {
  //     std::cout << "person id: " << i
  //                 << " , key point size: " << personvector.first.size()
  //                 << ", score: " << personvector.second << "\n" ;
  //     i++;
  // }

  return peopleVector;
}

// Delete people below the following thresholds:
// a) minSubsetCnt: removed if less than minSubsetCnt body parts
// b) minSubsetScore: removed if global score smaller than this
// c) maxPeaks (POSE_MAX_PEOPLE): keep first maxPeaks people above thresholds
void removePeopleBelowThresholds(
    std::vector<int> *validSubsetIndexes, int *numberPeople,
    const std::vector<std::pair<std::vector<int>, float>> &peopleVector,
    const unsigned int numberBodyParts, const int minSubsetCnt,
    const float minSubsetScore, const int maxPeaks,
    const bool maximizePositives) {
  *numberPeople = 0;
  validSubsetIndexes->clear();
  validSubsetIndexes->reserve(peopleVector.size());
  // Face valid sets
  std::vector<int> faceValidSubsetIndexes;
  faceValidSubsetIndexes.reserve(peopleVector.size());

  // Face invalid sets
  std::vector<int> faceInvalidSubsetIndexes;
  if (numberBodyParts >= 135)
    faceInvalidSubsetIndexes.reserve(peopleVector.size());

  validSubsetIndexes->reserve(fastMin((size_t)maxPeaks, peopleVector.size()));
  for (size_t index = 0; index < peopleVector.size(); index++) {
    auto personCounter = peopleVector[index].first.back();
    // Foot keypoints do not affect personCounter (too many false positives,
    // same foot usually appears as both left and right keypoints)
    // Pros: Removed tons of false positives
    // Cons: Standalone leg will never be recorded
    const auto personScore = peopleVector[index].second;
    if (personCounter >= minSubsetCnt &&
        (personScore / personCounter) >= minSubsetScore) {
      (*numberPeople)++;
      validSubsetIndexes->emplace_back(index);
      if (*numberPeople == maxPeaks)
        break;
    }
  }
}

void getPoseKeyPoints(
    std::vector<float> *poseKeypoints_ptr, const float scaleFactor,
    const std::vector<std::pair<std::vector<int>, float>> &peopleVector,
    const std::vector<int> &validSubsetIndexes, const float *const peaksPtr,
    const int numberPeople, const unsigned int numberBodyParts,
    const unsigned int numberBodyPartPairs) {
  if (numberPeople > 0)
    poseKeypoints_ptr->resize(numberPeople * numberBodyParts * 3);
  else
    poseKeypoints_ptr->clear();

  for (auto person = 0u; person < validSubsetIndexes.size(); person++) {
    const auto &peoplePair = peopleVector[validSubsetIndexes[person]].first;
    for (auto bodyPart = 0u; bodyPart < numberBodyParts; bodyPart++) {
      const auto baseOffset = (person * numberBodyParts + bodyPart) * 3;
      const auto bodyPartIndex = peoplePair[bodyPart];
      if (bodyPartIndex > 0) {
        poseKeypoints_ptr->at(baseOffset) =
            peaksPtr[bodyPartIndex - 2] * scaleFactor;
        poseKeypoints_ptr->at(baseOffset + 1) =
            peaksPtr[bodyPartIndex - 1] * scaleFactor;
        poseKeypoints_ptr->at(baseOffset + 2) = peaksPtr[bodyPartIndex];
      } else {
        poseKeypoints_ptr->at(baseOffset) = 0.f;
        poseKeypoints_ptr->at(baseOffset + 1) = 0.f;
        poseKeypoints_ptr->at(baseOffset + 2) = 0.f;
      }
    }
  }
}

void connectBodyParts(std::vector<float> *poseKeypoints_ptr,
                      const float *const heatMapPtr,
                      const float *const peaksPtr, const cv::Size &heatMapSize,
                      const int maxPeaks, const float interMinAboveThreshold,
                      const float interThreshold, const int minSubsetCnt,
                      const float minSubsetScore, const float scaleFactor,
                      const bool maximizePositives,
                      std::vector<int> *keypointShape) {
  // Parts Connection
  const std::vector<unsigned int> &bodyPartPairs = getPosePartPairs();
  unsigned int numberBodyParts = getNumberBodyParts();
  unsigned int numberBodyPartPairs = (unsigned int)(bodyPartPairs.size() / 2);

  // std::vector<std::pair<std::vector<int>, double>> refers to:
  //     - std::vector<int>: [body parts locations, #body parts found]
  //     - double: person subset score
  std::vector<std::pair<std::vector<int>, float>> peopleVector =
      createPeopleVector(heatMapPtr, peaksPtr, heatMapSize, maxPeaks,
                         interThreshold, interMinAboveThreshold, bodyPartPairs,
                         numberBodyParts, numberBodyPartPairs);

  // Delete people below the following thresholds:
  // a) minSubsetCnt: removed if less than minSubsetCnt body parts
  // b) minSubsetScore: removed if global score smaller than this
  // c) maxPeaks (POSE_MAX_PEOPLE): keep first maxPeaks people above
  // thresholds
  int numberPeople;
  std::vector<int> validSubsetIndexes;
  validSubsetIndexes.reserve(
      fastMin(static_cast<size_t>(maxPeaks), peopleVector.size()));
  removePeopleBelowThresholds(&validSubsetIndexes, &numberPeople, peopleVector,
                              numberBodyParts, minSubsetCnt, minSubsetScore,
                              maxPeaks, maximizePositives);

  // Fill and return poseKeypoints
  *keypointShape = {numberPeople, static_cast<int>(numberBodyParts), 3};

  // Fill and return poseKeypoints
  getPoseKeyPoints(poseKeypoints_ptr, scaleFactor, peopleVector,
                   validSubsetIndexes, peaksPtr, numberPeople, numberBodyParts,
                   numberBodyPartPairs);
}

void renderKeypoints(cv::Mat *frame, const std::vector<float> &keypoints,
                     const std::vector<int> keyshape,
                     const std::vector<unsigned int> &pairs,
                     const std::vector<float> colors,
                     const float thicknessCircleRatio,
                     const float thicknessLineRatioWRTCircle,
                     const float render_threshold, float scale) {
  // Get frame channels
  const auto width = frame->cols;
  const auto height = frame->rows;
  const auto area = width * height;

  // Parameters
  const auto lineType = 8;
  const auto shift = 0;
  const auto numberColors = colors.size();
  int numberKeypoints = keyshape[1];

  // Keypoints
  for (auto person = 0; person < keyshape[0]; person++) {
    const auto ratioAreas = 1;
    // Size-dependent variables
    const auto thicknessRatio = fastMax(
        intRound(std::sqrt(area) * thicknessCircleRatio * ratioAreas), 1);
    // Negative thickness in cv::circle means that a filled circle is to be
    // drawn.
    const auto thicknessCircle = (ratioAreas > 0.05 ? thicknessRatio : -1);
    const auto thicknessLine =
        intRound(thicknessRatio * thicknessLineRatioWRTCircle);
    const auto radius = thicknessRatio / 2;

    // Draw lines
    for (auto pair = 0u; pair < pairs.size(); pair += 2) {
      const auto index1 =
          (person * numberKeypoints + pairs[pair]) * keyshape[2];
      const auto index2 =
          (person * numberKeypoints + pairs[pair + 1]) * keyshape[2];
      if (keypoints[index1 + 2] > render_threshold &&
          keypoints[index2 + 2] > render_threshold) {
        const auto colorIndex =
            pairs[pair + 1] * 3;  // Before: colorIndex = pair/2*3;
        const cv::Scalar color{colors[(colorIndex + 2) % numberColors],
                               colors[(colorIndex + 1) % numberColors],
                               colors[(colorIndex + 0) % numberColors]};
        const cv::Point keypoint1{intRound(keypoints[index1] * scale),
                                  intRound(keypoints[index1 + 1] * scale)};
        const cv::Point keypoint2{intRound(keypoints[index2] * scale),
                                  intRound(keypoints[index2 + 1] * scale)};
        cv::line(*frame, keypoint1, keypoint2, color, thicknessLine, lineType,
                 shift);
      }
    }

    // Draw circles on key points
    for (int part = 0; part < numberKeypoints; part++) {
      const int faceIndex = (person * numberKeypoints + part) * keyshape[2];
      if (keypoints[faceIndex + 2] > render_threshold) {
        const int colorIndex = part * 3;
        const cv::Scalar color{colors[(colorIndex + 2) % numberColors],
                               colors[(colorIndex + 1) % numberColors],
                               colors[(colorIndex + 0) % numberColors]};
        const cv::Point center{intRound(keypoints[faceIndex] * scale),
                               intRound(keypoints[faceIndex + 1] * scale)};
        cv::circle(*frame, center, radius, color, thicknessCircle, lineType,
                   shift);
      }
    }
  }
}

void renderPoseKeypoints(cv::Mat *frame,
                         const std::vector<float> &poseKeypoints,
                         const std::vector<int> keyshape,
                         const float renderThreshold, float scale,
                         const bool blendOriginalFrame) {
  // if not blend original frame, set frame data to 0(black frame)
  if (!blendOriginalFrame)
    frame->setTo(0.f);

  // Parameters
  const float thicknessCircleRatio = 1.f / 75.f;
  const float thicknessLineRatioWRTCircle = 0.75f;
  const std::vector<unsigned int> pairs = POSE_COCO_BODY_PART_PAIRS;

  // Render keypoints
  renderKeypoints(frame, poseKeypoints, keyshape, pairs,
                  POSE_COCO_COLORS_RENDER, thicknessCircleRatio,
                  thicknessLineRatioWRTCircle, renderThreshold, scale);
}
}  // namespace openpose
