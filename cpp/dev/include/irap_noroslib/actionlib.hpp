// actionlib.hpp — SimpleActionClient<Act> / SimpleActionServer<Act> over the
// five standard action topics (goal/cancel/status/feedback/result).
//
// `Act` is a traits struct exposing the nested message types Goal, Result,
// Feedback, ActionGoal, ActionResult, ActionFeedback (each a normal irap_noroslib
// message). See examples/fibonacci_action.hpp for the pattern. Interoperates
// with real ROS action servers/clients (verified vs actionlib_tutorials).
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "irap_noroslib/actionlib_msgs.hpp"
#include "irap_noroslib/node.hpp"

namespace irap_noroslib {

using GoalStatusVals = actionlib_msgs::GoalStatus;   // PENDING..LOST enum

namespace detail {
inline std::string new_goal_id() {
  static std::atomic<uint64_t> counter{0};
  timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
  return "irap_noroslib_goal-" + std::to_string(counter.fetch_add(1)) + "-" +
         std::to_string(ts.tv_sec) + "." + std::to_string(ts.tv_nsec);
}
inline void stamp_now(uint32_t* sec, uint32_t* nsec) {
  timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
  *sec = (uint32_t)ts.tv_sec; *nsec = (uint32_t)ts.tv_nsec;
}
}  // namespace detail

// --- SimpleActionClient<Act> ----------------------------------------------
template <typename Act>
class SimpleActionClient {
 public:
  using Feedback = typename Act::Feedback;
  using Result = typename Act::Result;
  using Goal = typename Act::Goal;
  using FeedbackCb = std::function<void(const Feedback&)>;

  explicit SimpleActionClient(const std::string& ns)
      : ns_(ns[0] == '/' ? ns : "/" + ns),
        goal_pub_(ns_ + "/goal"),
        cancel_pub_(ns_ + "/cancel"),
        status_sub_(ns_ + "/status", [this](const actionlib_msgs::GoalStatusArray& m) { on_status(m); }),
        fb_sub_(ns_ + "/feedback", [this](const typename Act::ActionFeedback& m) { on_feedback(m); }),
        res_sub_(ns_ + "/result", [this](const typename Act::ActionResult& m) { on_result(m); }) {}

  // The goal-resend thread captures `this`, so it must not outlive us.
  ~SimpleActionClient() {
    { std::lock_guard<std::mutex> lk(mu_); done_ = true; }   // stop the resend loop
    if (resend_.joinable()) resend_.join();
  }
  SimpleActionClient(const SimpleActionClient&) = delete;
  SimpleActionClient& operator=(const SimpleActionClient&) = delete;

  bool waitForServer(double timeout_s = -1) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(timeout_s < 0 ? 0 : timeout_s));
    while (ok()) {
      if (goal_pub_.get_num_connections() > 0 && seen_status_.load()) return true;
      if (timeout_s >= 0 && std::chrono::steady_clock::now() >= deadline) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
  }

  void sendGoal(const Goal& goal, FeedbackCb feedback_cb = nullptr) {
    typename Act::ActionGoal ag;
    detail::stamp_now(&ag.header.stamp_sec, &ag.header.stamp_nsec);
    ag.goal_id.id = detail::new_goal_id();
    detail::stamp_now(&ag.goal_id.stamp_sec, &ag.goal_id.stamp_nsec);
    ag.goal = goal;
    if (resend_.joinable()) resend_.join();      // a previous goal's resender
    {
      std::lock_guard<std::mutex> lk(mu_);
      goal_id_ = ag.goal_id.id;
      feedback_cb_ = std::move(feedback_cb);
      state_ = GoalStatusVals::PENDING;
      done_ = false;
    }
    // Resend until the server acks (status shows our id) or a result arrives,
    // covering the pub/sub connection race that would otherwise drop the goal.
    // Joined in the destructor: it captures `this`, so it must not outlive us.
    resend_ = std::thread([this, ag] {
      for (int i = 0; i < 50; ++i) {
        goal_pub_.publish(ag);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lk(mu_);
        if (done_ || state_ != GoalStatusVals::PENDING) return;
      }
    });
  }

  void cancelGoal() {
    actionlib_msgs::GoalID g;
    { std::lock_guard<std::mutex> lk(mu_); g.id = goal_id_; }
    detail::stamp_now(&g.stamp_sec, &g.stamp_nsec);
    cancel_pub_.publish(g);
  }

  bool waitForResult(double timeout_s = -1) {
    std::unique_lock<std::mutex> lk(mu_);
    if (timeout_s < 0) { cv_.wait(lk, [this] { return done_; }); return true; }
    return cv_.wait_for(lk, std::chrono::duration<double>(timeout_s), [this] { return done_; });
  }

  Result getResult() { std::lock_guard<std::mutex> lk(mu_); return result_; }
  uint8_t getState() { std::lock_guard<std::mutex> lk(mu_); return state_; }

 private:
  void on_status(const actionlib_msgs::GoalStatusArray& m) {
    seen_status_.store(true);
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& st : m.status_list)
      if (st.goal_id.id == goal_id_) state_ = st.status;
  }
  void on_feedback(const typename Act::ActionFeedback& m) {
    FeedbackCb cb;
    { std::lock_guard<std::mutex> lk(mu_);
      if (m.status.goal_id.id != goal_id_) return;
      cb = feedback_cb_; }
    if (cb) cb(m.feedback);
  }
  void on_result(const typename Act::ActionResult& m) {
    { std::lock_guard<std::mutex> lk(mu_);
      if (m.status.goal_id.id != goal_id_) return;
      result_ = m.result; state_ = m.status.status; done_ = true; }
    cv_.notify_all();
  }

  std::string ns_;
  Publisher<typename Act::ActionGoal> goal_pub_;
  Publisher<actionlib_msgs::GoalID> cancel_pub_;
  Subscriber<actionlib_msgs::GoalStatusArray> status_sub_;
  Subscriber<typename Act::ActionFeedback> fb_sub_;
  Subscriber<typename Act::ActionResult> res_sub_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::string goal_id_;
  FeedbackCb feedback_cb_;
  Result result_{};
  uint8_t state_ = GoalStatusVals::PENDING;
  bool done_ = false;
  std::atomic<bool> seen_status_{false};
  std::thread resend_;
};

// --- SimpleActionServer<Act> ----------------------------------------------
template <typename Act>
class SimpleActionServer {
 public:
  using Goal = typename Act::Goal;
  using Result = typename Act::Result;
  using Feedback = typename Act::Feedback;
  // execute_cb(goal, server&): call publishFeedback / setSucceeded / setAborted.
  using ExecuteCb = std::function<void(const Goal&, SimpleActionServer<Act>&)>;

  SimpleActionServer(const std::string& ns, ExecuteCb cb)
      : ns_(ns[0] == '/' ? ns : "/" + ns), execute_(std::move(cb)),
        status_pub_(ns_ + "/status"),
        result_pub_(ns_ + "/result"),
        feedback_pub_(ns_ + "/feedback"),
        goal_sub_(ns_ + "/goal", [this](const typename Act::ActionGoal& m) { on_goal(m); }),
        cancel_sub_(ns_ + "/cancel", [this](const actionlib_msgs::GoalID& m) { on_cancel(m); }) {
    running_.store(true);
    status_thread_ = std::thread([this] { status_loop(); });
  }
  ~SimpleActionServer() {
    running_.store(false);
    if (status_thread_.joinable()) status_thread_.join();
  }

  bool isPreemptRequested() { std::lock_guard<std::mutex> lk(mu_); return preempt_; }

  void publishFeedback(const Feedback& fb) {
    typename Act::ActionFeedback m;
    detail::stamp_now(&m.header.stamp_sec, &m.header.stamp_nsec);
    { std::lock_guard<std::mutex> lk(mu_);
      m.status.goal_id.id = cur_id_; m.status.status = GoalStatusVals::ACTIVE; }
    m.feedback = fb;
    feedback_pub_.publish(m);
  }
  void setSucceeded(const Result& r = Result()) { finish(GoalStatusVals::SUCCEEDED, r); }
  void setAborted(const Result& r = Result()) { finish(GoalStatusVals::ABORTED, r); }
  void setPreempted(const Result& r = Result()) { finish(GoalStatusVals::PREEMPTED, r); }

 private:
  void status_loop() {
    Rate rate(10);
    while (running_.load() && ok()) { publish_status(); rate.sleep(); }
  }
  void publish_status() {
    actionlib_msgs::GoalStatusArray arr;
    detail::stamp_now(&arr.header.stamp_sec, &arr.header.stamp_nsec);
    { std::lock_guard<std::mutex> lk(mu_);
      if (!cur_id_.empty()) {
        actionlib_msgs::GoalStatus st; st.goal_id.id = cur_id_; st.status = cur_status_;
        arr.status_list.push_back(st);
      } }
    status_pub_.publish(arr);
  }
  void on_goal(const typename Act::ActionGoal& ag) {
    { std::lock_guard<std::mutex> lk(mu_);
      if (ag.goal_id.id == cur_id_) return;   // duplicate resend
      cur_id_ = ag.goal_id.id; cur_status_ = GoalStatusVals::ACTIVE; preempt_ = false; }
    Goal goal = ag.goal;
    std::thread([this, goal] {
      execute_(goal, *this);
      std::lock_guard<std::mutex> lk(mu_);
      if (cur_status_ == GoalStatusVals::ACTIVE) {   // handler forgot to finish
        cur_status_ = GoalStatusVals::ABORTED;
      }
    }).detach();
  }
  void on_cancel(const actionlib_msgs::GoalID& g) {
    std::lock_guard<std::mutex> lk(mu_);
    if (g.id == cur_id_ || g.id.empty()) preempt_ = true;
  }
  void finish(uint8_t status, const Result& r) {
    typename Act::ActionResult m;
    detail::stamp_now(&m.header.stamp_sec, &m.header.stamp_nsec);
    { std::lock_guard<std::mutex> lk(mu_);
      m.status.goal_id.id = cur_id_; m.status.status = status; cur_status_ = status; }
    m.result = r;
    result_pub_.publish(m);
    publish_status();
  }

  std::string ns_;
  ExecuteCb execute_;
  Publisher<actionlib_msgs::GoalStatusArray> status_pub_;
  Publisher<typename Act::ActionResult> result_pub_;
  Publisher<typename Act::ActionFeedback> feedback_pub_;
  Subscriber<typename Act::ActionGoal> goal_sub_;
  Subscriber<actionlib_msgs::GoalID> cancel_sub_;
  std::mutex mu_;
  std::string cur_id_;
  uint8_t cur_status_ = GoalStatusVals::PENDING;
  bool preempt_ = false;
  std::atomic<bool> running_{false};
  std::thread status_thread_;
};

}  // namespace irap_noroslib
