// noros webcam publisher (C++): capture /dev/video0 with OpenCV and publish BOTH
// a raw sensor_msgs/Image (bgr8) and a sensor_msgs/CompressedImage (jpeg) to a
// real roscore — no ROS linked on this side.
//
//     /noros/image_raw              sensor_msgs/Image
//     /noros/image_raw/compressed   sensor_msgs/CompressedImage
//
//   ./webcam_pub          (with a roscore running)
//   ./image_viewer        (the noros C++ viewer) or ros_image_viewer.py (real ROS)
//
// REQUIRES OpenCV (cv::VideoCapture / cv::imencode). OpenCV is NOT a dependency
// of noros itself — the core library links zero third-party packages; only this
// optional demo needs it. Build:
//   g++ -std=c++17 webcam_pub.cpp noros_impl.cpp -o webcam_pub \
//       -pthread $(pkg-config --cflags --libs opencv4)
#include "noros.hpp"
#include <opencv2/opencv.hpp>
#include <cstdlib>

int main(int argc, char** argv) {
  // Point noros at the ROS master before init_node (defaults to a local roscore).
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("noros_webcam");

  int dev = argc > 1 ? std::atoi(argv[1]) : 0;
  cv::VideoCapture cap(dev);
  if (!cap.isOpened()) { noros::logerr("cannot open /dev/video" + std::to_string(dev)); return 1; }
  cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
  cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

  noros::Publisher<sensor_msgs::Image> raw_pub("/noros/image_raw");
  noros::Publisher<sensor_msgs::CompressedImage> comp_pub("/noros/image_raw/compressed");
  noros::Rate rate(30);
  uint32_t seq = 0;
  cv::Mat frame;
  while (noros::ok()) {
    if (!cap.read(frame) || frame.empty()) { noros::logwarn("frame grab failed"); rate.sleep(); continue; }
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
      noros::loginfo("seq=" + std::to_string(seq) + " " + std::to_string(w) + "x" +
                     std::to_string(h) + " raw=" + std::to_string(raw.data.size()) +
                     " B jpeg=" + std::to_string(enc.size()) + " B");
    ++seq;
    rate.sleep();
  }
  return 0;
}
