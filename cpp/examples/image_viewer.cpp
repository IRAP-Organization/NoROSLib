// irap_noroslib image viewer (C++): subscribe to the images webcam_pub publishes and show
// them with OpenCV. This is the C++ counterpart of ros_image_viewer.py — except
// that one is a real rospy node, whereas this receives the frames through irap_noroslib
// (no ROS linked) and only uses OpenCV to decode + display.
//
//     /irap_noroslib/image_raw              sensor_msgs/Image           (bgr8)
//     /irap_noroslib/image_raw/compressed   sensor_msgs/CompressedImage (jpeg)
//
//   ./webcam_pub          (publisher, with a roscore running)
//   ./image_viewer        (this)
//
// REQUIRES OpenCV (cv::imshow / cv::imdecode). OpenCV is NOT a dependency of irap_noroslib
// itself — only this optional demo needs it. Build:
//   g++ -std=c++17 image_viewer.cpp irap_noroslib_impl.cpp -o image_viewer \
//       -pthread $(pkg-config --cflags --libs opencv4)
#include "irap_noroslib.hpp"
#include "irap_noroslib/sensor_msgs/Image.h"
#include "irap_noroslib/sensor_msgs/CompressedImage.h"
#include <opencv2/opencv.hpp>
#include <atomic>
#include <mutex>
#include <cstdlib>

int main() {
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  irap_noroslib::set_master_uri(mu ? mu : "http://localhost:11311");
  irap_noroslib::set_hostname(hn ? hn : "localhost");
  irap_noroslib::init_node("irap_noroslib_image_viewer");

  std::mutex mu_frames;
  cv::Mat raw_img, comp_img;
  std::atomic<int> nraw{0}, ncomp{0};

  irap_noroslib::Subscriber<sensor_msgs::Image> raw_sub("/irap_noroslib/image_raw",
      [&](const sensor_msgs::Image& m) {
        if (m.encoding != "bgr8" || m.data.size() < (size_t)m.height * m.width * 3) return;
        cv::Mat img(m.height, m.width, CV_8UC3, const_cast<uint8_t*>(m.data.data()));
        std::lock_guard<std::mutex> lk(mu_frames); img.copyTo(raw_img); ++nraw;
      });

  irap_noroslib::Subscriber<sensor_msgs::CompressedImage> comp_sub("/irap_noroslib/image_raw/compressed",
      [&](const sensor_msgs::CompressedImage& m) {
        cv::Mat buf(1, (int)m.data.size(), CV_8UC1, const_cast<uint8_t*>(m.data.data()));
        cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (img.empty()) return;
        std::lock_guard<std::mutex> lk(mu_frames); comp_img = img; ++ncomp;
      });

  const bool gui = std::getenv("DISPLAY") != nullptr;
  irap_noroslib::loginfo(std::string("irap_noroslib image viewer up; gui=") + (gui ? "yes" : "no (headless)"));
  irap_noroslib::Rate rate(50);
  int ticks = 0;
  while (irap_noroslib::ok()) {
    if (gui) {
      std::lock_guard<std::mutex> lk(mu_frames);
      if (!raw_img.empty()) cv::imshow("irap_noroslib raw (C++)", raw_img);
      if (!comp_img.empty()) cv::imshow("irap_noroslib compressed (C++)", comp_img);
      cv::waitKey(1);
    }
    if (++ticks % 100 == 0)
      irap_noroslib::loginfo("raw=" + std::to_string(nraw.load()) + " compressed=" + std::to_string(ncomp.load()));
    rate.sleep();
  }
  return 0;
}
