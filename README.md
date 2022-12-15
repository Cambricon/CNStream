# Cambricon CNStream #
CNStream is a streaming framework with plug-ins. It is used to connect other modules, includes basic functionalities, libraries,
and essential elements.

CNStream provides the following built-in modules:

- DataSource: Support RTSP, video file, images, elementary stream in memory and sensor inputs (H.264, H.265, and JPEG decoding) (sensor input is only supported on edge platforms).
- Inferencer: MLU-based inference accelerator for detection and classification, based on EasyDK InferServer.
- Osd (On-screen display): Module for highlighting objects and text overlay.
- VEncode: Encode videos or images and write to file or push RTSP stream to internet.
- Vout: Display the video on screen (Only support on edge platforms).
- Tracker: Multi-object tracking.

### Getting started ###

  To start using CNStream, please refer to the chapter of ***quick start*** in the document of [Cambricon-CNStream-User-Guide-CN.pdf](./docs/release_document/latest/Cambricon-CNStream-User-Guide-CN-vlatest.pdf) .
## Samples ##

|                        Classification                        |               Object Detection                |
| :----------------------------------------------------------: | :-------------------------------------------: |
| <img src="./data/gifs/image_classification.gif" alt="Classification" style="height=350px" /> | <img src="./data/gifs/object_detection_yolov3.gif" alt="Object Detection" style="height=350px" /> |

|               Object Tracking               |               License plate recognition               |
| :-----------------------------------------: | :-----------------------------------------------------: |
| <img src="./data/gifs/object_tracking.gif" alt="Object Tracking" style="height=350px" /> | <img src="./data/gifs/lpr.gif" alt="License plate recognition" style="height=350px" /> |

|                           Body Pose                          |
| :----------------------------------------------------------: |
| <img src="./data/gifs/body_pose.gif" alt="Body Pose" style="height=350px" /> |

## Best Practices ##

### **How to change the input video file?** ##

Modify the `files.list_video` file, which is under the `samples` directory, to replace the video path. Each line represents one stream. It is recommended to use an absolute path or use a relative path relative to the executor path.


## Documentation ##
[Cambricon Forum Docs](https://www.cambricon.com/docs/cnstream/user_guide_html/index.html)

Check out the Examples page for tutorials on how to use CNStream. Concepts page for basic definitions.

## Community forum ##
[Discuss](http://forum.cambricon.com/list-47-1.html) - General community discussion around CNStream.
