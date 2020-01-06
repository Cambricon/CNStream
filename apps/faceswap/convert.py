from interface.faceswap import Faceswap, TMP_DIR
import cv2
import argparse
import os
import logging
logging.basicConfig(level = logging.INFO,format = '%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class Convert(object):
    def __init__(self, args):
        self.args = args
        self.image_path = "./test.jpg"
        self.faceswap_model = Faceswap(
            blur_size=self.args.blur_size,
            seamless_clone=self.args.seamless,
            mask_type=self.args.mask_type,
            erosion_kernel_size=self.args.erosion_kernel_size)


    def run(self):
        i = 0
        start = cv2.getTickCount()
        if not os.path.exists(self.args.input):
            logger.error("input file doesn't exit")
            return
        if self.args.type == "image":
            image = cv2.imread(self.args.input)
            result = self.faceswap_model.predict(image, "image")
            if self.args.output is None:
                cv2.imwrite(os.path.join(TMP_DIR, "result.jpg"), result)
            else:
                cv2.imwrite(self.args.output, result)
        elif self.args.type == "video":
            self.faceswap_model.process_of_video(self.args.input)
            videoCapture = cv2.VideoCapture(self.args.input)
            fps = videoCapture.get(cv2.CAP_PROP_FPS)
            size = (int(videoCapture.get(cv2.CAP_PROP_FRAME_WIDTH)),
                    int(videoCapture.get(cv2.CAP_PROP_FRAME_HEIGHT)))
            if self.args.output is None:
                videoWrite = cv2.VideoWriter(os.path.join(TMP_DIR, "result.avi"),
                                            cv2.VideoWriter_fourcc("X", "V", "I", "D"),
                                            fps, size)
            else:
                videoWrite = cv2.VideoWriter(self.args.output,
                                            cv2.VideoWriter_fourcc("X", "V", "I", "D"),
                                            fps, size)
            sucess, frame = videoCapture.read()
            while sucess:
                result = self.faceswap_model.predict(frame, "video")
                videoWrite.write(result)
                i += 1
                logger.info("posrprocess: processing {}".format(i))
                sucess, frame = videoCapture.read()
            videoWrite.release()
        else:
            logger.error("error type, please choose the type of file: image or video")
            return
            
        self.faceswap_model.video_capture = None
        end = cv2.getTickCount()
        logger.info("cost time : {}".format((end - start)/cv2.getTickFrequency()))



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Demo of faceswap")
    parser.add_argument("-i", "--input")
    parser.add_argument("-o", "--output",
                            default=None)
    parser.add_argument("-t", "--type",
                            choices=["video", "image"])
    parser.add_argument("-b", "--blur-size", default=2)
    parser.add_argument("-S", "--seamless", default=False)
    parser.add_argument('-M', '--mask-type',
                            type=str.lower,
                            choices=["rect", "facehull", "facehullandrect"],
                            default="facehullandrect")
    parser.add_argument('-e', '--erosion-kernel-size',
                            type=int,
                            default=None)
    args = parser.parse_args()
    convert = Convert(args)
    convert.run()