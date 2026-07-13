// irap_noroslib webcam publisher (C++): capture /dev/video0 with OpenCV and publish BOTH
// a raw sensor_msgs/Image (bgr8) and a sensor_msgs/CompressedImage (jpeg) to a
// real roscore — no ROS linked on this side.
//
//     /irap_noroslib/image_raw              sensor_msgs/Image
//     /irap_noroslib/image_raw/compressed   sensor_msgs/CompressedImage
//
//   ./webcam_pub          (with a roscore running)
//   ./image_viewer        (the irap_noroslib C++ viewer) or ros_image_viewer.py (real ROS)
//
// REQUIRES OpenCV (cv::VideoCapture / cv::imencode). OpenCV is NOT a dependency
// of irap_noroslib itself — the core library links zero third-party packages; only this
// optional demo needs it. Build:
//   g++ -std=c++17 webcam_pub.cpp irap_noroslib_impl.cpp -o webcam_pub \
//       -pthread $(pkg-config --cflags --libs opencv4)
#include "irap_noroslib.hpp"
#include "irap_noroslib/sensor_msgs/Image.h"
#include "irap_noroslib/sensor_msgs/CompressedImage.h"
#include <opencv2/opencv.hpp>
#include <cstdlib>

int main(int argc, char** argv) {
  // Point irap_noroslib at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_webcam");

  int dev = argc > 1 ? std::atoi(argv[1]) : 0;
  cv::VideoCapture cap(dev);
  if (!cap.isOpened()) { irap_noroslib::logerr("cannot open /dev/video" + std::to_string(dev)); return 1; }
  cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
  cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

  irap_noroslib::Publisher<sensor_msgs::Image> raw_pub("/irap_noroslib/image_raw");
  irap_noroslib::Publisher<sensor_msgs::CompressedImage> comp_pub("/irap_noroslib/image_raw/compressed");
  irap_noroslib::Rate rate(30);
  uint32_t seq = 0;
  cv::Mat frame;
  while (irap_noroslib::ok()) {
    if (!cap.read(frame) || frame.empty()) { irap_noroslib::logwarn("frame grab failed"); rate.sleep(); continue; }
    int h = frame.rows, w = frame.cols;

    // --- raw Image (bgr8) ---
    sensor_msgs::Image raw;
    raw.header.seq = seq; raw.header.stamp_now(); raw.header.frame_id = "camera";
    raw.height = h; raw.width = w; raw.encoding = "bgr8"; raw.is_bigendian = 0;
    raw.step = w * 3;
    cv::Mat cont = frame.isContinuous() ? frame : frame.clone();
    raw.data.assign(cont.data, cont.data + (size_t)h * w * 3);
    raw_pub.publish(raw);

    // --- CompressedImage (jpeg) ---
    std::vector<uint8_t> enc;
    if (cv::imencode(".jpg", frame, enc, {cv::IMWRITE_JPEG_QUALITY, 80})) {
      sensor_msgs::CompressedImage comp;
      comp.header.seq = seq; comp.header.stamp_now(); comp.header.frame_id = "camera";
      comp.format = "jpeg"; comp.data = enc;
      comp_pub.publish(comp);
    }

    if (seq % 15 == 0)
      irap_noroslib::loginfo("seq=" + std::to_string(seq) + " " + std::to_string(w) + "x" +
                     std::to_string(h) + " raw=" + std::to_string(raw.data.size()) +
                     " B jpeg=" + std::to_string(enc.size()) + " B");
    ++seq;
    rate.sleep();
  }
  return 0;
}
