// noros image viewer (C++): subscribe to the images webcam_pub publishes and show
// them with OpenCV. This is the C++ counterpart of ros_image_viewer.py — except
// that one is a real rospy node, whereas this receives the frames through noros
// (no ROS linked) and only uses OpenCV to decode + display.
//
//     /noros/image_raw              sensor_msgs/Image           (bgr8)
//     /noros/image_raw/compressed   sensor_msgs/CompressedImage (jpeg)
//
//   ./webcam_pub          (publisher, with a roscore running)
//   ./image_viewer        (this)
//
// REQUIRES OpenCV (cv::imshow / cv::imdecode). OpenCV is NOT a dependency of noros
// itself — only this optional demo needs it. Build:
//   g++ -std=c++17 image_viewer.cpp noros_impl.cpp -o image_viewer \
//       -pthread $(pkg-config --cflags --libs opencv4)
#include "noros.hpp"
#include <opencv2/opencv.hpp>
#include <atomic>
#include <mutex>
#include <cstdlib>

int main() {
  const char* mu = std::getenv("ROS_MASTER_URI");
  const char* hn = std::getenv("ROS_HOSTNAME");
  noros::set_master_uri(mu ? mu : "http://localhost:11311");
  noros::set_hostname(hn ? hn : "localhost");
  noros::init_node("noros_image_viewer");

  std::mutex mu_frames;
  cv::Mat raw_img, comp_img;
  std::atomic<int> nraw{0}, ncomp{0};

  noros::Subscriber<sensor_msgs::Image> raw_sub("/noros/image_raw",
      [&](const sensor_msgs::Image& m) {
        if (m.encoding != "bgr8" || m.data.size() < (size_t)m.height * m.width * 3) return;
        cv::Mat img(m.height, m.width, CV_8UC3, const_cast<uint8_t*>(m.data.data()));
        std::lock_guard<std::mutex> lk(mu_frames); img.copyTo(raw_img); ++nraw;
      });

  noros::Subscriber<sensor_msgs::CompressedImage> comp_sub("/noros/image_raw/compressed",
      [&](const sensor_msgs::CompressedImage& m) {
        cv::Mat buf(1, (int)m.data.size(), CV_8UC1, const_cast<uint8_t*>(m.data.data()));
        cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (img.empty()) return;
        std::lock_guard<std::mutex> lk(mu_frames); comp_img = img; ++ncomp;
      });

  const bool gui = std::getenv("DISPLAY") != nullptr;
  noros::loginfo(std::string("noros image viewer up; gui=") + (gui ? "yes" : "no (headless)"));
  noros::Rate rate(50);
  int ticks = 0;
  while (noros::ok()) {
    if (gui) {
      std::lock_guard<std::mutex> lk(mu_frames);
      if (!raw_img.empty()) cv::imshow("noros raw (C++)", raw_img);
      if (!comp_img.empty()) cv::imshow("noros compressed (C++)", comp_img);
      cv::waitKey(1);
    }
    if (++ticks % 100 == 0)
      noros::loginfo("raw=" + std::to_string(nraw.load()) + " compressed=" + std::to_string(ncomp.load()));
    rate.sleep();
  }
  return 0;
}
