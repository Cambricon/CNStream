#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "easyinfer/model_loader.h"
#include "reflex_object.h"

#include "cnstream_frame.hpp"
#include "postproc.hpp"

using std::cerr;
using std::endl;
using std::pair;
using std::to_string;
using std::vector;

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

class PostprocStyle_transfer : public cnstream::Postproc {
 public:
  /**
   * @brief Execute postproc on neural style_transfer network outputs
   *
   * @param net_outputs: neural network outputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store processed result
   *
   * @return return 0 if succeed
   */
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(PostprocStyle_transfer, cnstream::Postproc)
};  // class PostprocStyle_transfer

IMPLEMENT_REFLEX_OBJECT_EX(PostprocStyle_transfer, cnstream::Postproc)

int PostprocStyle_transfer::Execute(const std::vector<float*>& net_outputs,
                                    const std::shared_ptr<edk::ModelLoader>& model,
                                    const cnstream::CNFrameInfoPtr& package) {
  if (net_outputs.size() != 1) {
    std::cerr << "[Warnning] Style_transfer neuron network only hs one output, "
                 "but get " +
                     std::to_string(net_outputs.size()) + "\n";
    return -1;
  }
  auto sp = model->OutputShapes()[0];
  int im_w = sp.w;
  int im_h = sp.h;
  auto pdata = net_outputs[0];
  cv::Mat i(im_w, im_h, CV_32FC1, pdata);
  std::vector<cv::Mat> mRGB(3), mBGR(3);
  for (int i = 0; i < 3; i++) {
    cv::Mat img(im_w, im_h, CV_32FC1, pdata + im_w * im_h * i);
    mBGR[i] = img;
  }
  cv::Mat R, G, B;
  mBGR[0].convertTo(R, CV_8UC1);
  mBGR[1].convertTo(G, CV_8UC1);
  mBGR[2].convertTo(B, CV_8UC1);

  mRGB[0] = R;
  mRGB[1] = G;
  mRGB[2] = B;
  cv::Mat img_merge(im_w, im_h, CV_32FC3);
  cv::merge(mRGB, img_merge);
  // static int out = 0;
  // std::string out_name = "output/" + std::to_string(out++) + ".jpg";
  std::string out_name = "style.jpg";
  imwrite(out_name, img_merge);
  return 0;
}
