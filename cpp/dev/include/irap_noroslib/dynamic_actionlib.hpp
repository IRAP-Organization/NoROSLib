// dynamic_actionlib.hpp -- actionlib with a type loaded from a `.action` FILE.
//
// Same five topics and same semantics as SimpleActionClient/Server, but the goal,
// feedback and result types come from ActionType instead of a traits struct. Only
// three of the five topics are actually type-dependent: /cancel is always an
// actionlib_msgs/GoalID and /status always an actionlib_msgs/GoalStatusArray, so
// those keep the compile-time Publisher/Subscriber.
//
//   ActionType A = load_action_file("/home/me/msgs/Fibonacci.action", "my_pkg");
//   DynamicActionClient c("/fibonacci", A);
//   c.waitForServer();
//   DynamicMessage goal = c.createGoal();
//   goal.set("order", 10);
//   c.sendGoal(goal, [](const DynamicMessage& fb) { ... });
//   c.waitForResult();
//   auto seq = c.getResult().get_array<int32_t>("sequence");
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "irap_noroslib/actionlib.hpp"
#include "irap_noroslib/dynamic_node.hpp"

namespace irap_noroslib {

class DynamicActionClient {
 public:
  using FeedbackCb = std::function<void(const DynamicMessage&)>;   // <T>Feedback

  DynamicActionClient(const std::string& ns, const ActionType& act)
      : ns_(ns[0] == '/' ? ns : "/" + ns),
        act_(act),
        goal_pub_(ns_ + "/goal", act.action_goal()),
        cancel_pub_(ns_ + "/cancel"),
        status_sub_(ns_ + "/status",
                    [this](const actionlib_msgs::GoalStatusArray& m) { on_status(m); }),
        fb_sub_(ns_ + "/feedback", act.action_feedback(),
                [this](const DynamicMessage& m) { on_feedback(m); }),
        res_sub_(ns_ + "/result", act.action_result(),
                 [this](const DynamicMessage& m) { on_result(m); }),
        result_(act.result().create()) {}

  // The goal-resend thread captures `this`, so it must not outlive us.
  ~DynamicActionClient() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      done_ = true;                 // tells the resend loop to stop
    }
    if (resend_.joinable()) resend_.join();
  }

  DynamicActionClient(const DynamicActionClient&) = delete;
  DynamicActionClient& operator=(const DynamicActionClient&) = delete;

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

  /// A blank goal of this action's type, ready to fill in.
  DynamicMessage createGoal() const { return act_.goal().create(); }

  void sendGoal(const DynamicMessage& goal, FeedbackCb feedback_cb = nullptr) {
    DynamicMessage ag = act_.action_goal().create();
    uint32_t s, n;
    detail::stamp_now(&s, &n);
    ag.set("header.stamp", Time{s, n});
    std::string id = detail::new_goal_id();
    ag.set("goal_id.id", id);
    ag.set("goal_id.stamp", Time{s, n});
    ag.set("goal", goal);
    if (resend_.joinable()) resend_.join();      // a previous goal's resender
    {
      std::lock_guard<std::mutex> lk(mu_);
      goal_id_ = id;
      feedback_cb_ = std::move(feedback_cb);
      state_ = GoalStatusVals::PENDING;
      done_ = false;
    }
    // Resend until the server acks (status carries our id) or a result lands --
    // covers the pub/sub connection race that would otherwise drop the goal.
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
    return cv_.wait_for(lk, std::chrono::duration<double>(timeout_s),
                        [this] { return done_; });
  }

  DynamicMessage getResult() { std::lock_guard<std::mutex> lk(mu_); return result_; }
  uint8_t getState() { std::lock_guard<std::mutex> lk(mu_); return state_; }

 private:
  void on_status(const actionlib_msgs::GoalStatusArray& m) {
    seen_status_.store(true);
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& st : m.status_list)
      if (st.goal_id.id == goal_id_) state_ = st.status;
  }
  void on_feedback(const DynamicMessage& m) {
    FeedbackCb cb;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (m.get<std::string>("status.goal_id.id") != goal_id_) return;
      cb = feedback_cb_;
    }
    if (cb) cb(m.msg("feedback"));
  }
  void on_result(const DynamicMessage& m) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (m.get<std::string>("status.goal_id.id") != goal_id_) return;
      result_ = m.msg("result");
      state_ = m.get<uint8_t>("status.status");
      done_ = true;
    }
    cv_.notify_all();
  }

  std::string ns_;
  ActionType act_;
  DynamicPublisher goal_pub_;
  Publisher<actionlib_msgs::GoalID> cancel_pub_;
  Subscriber<actionlib_msgs::GoalStatusArray> status_sub_;
  DynamicSubscriber fb_sub_, res_sub_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::string goal_id_;
  FeedbackCb feedback_cb_;
  DynamicMessage result_;
  uint8_t state_ = GoalStatusVals::PENDING;
  bool done_ = false;
  std::atomic<bool> seen_status_{false};
  std::thread resend_;
};

class DynamicActionServer {
 public:
  // execute_cb(goal, server&): call publishFeedback / setSucceeded / setAborted.
  using ExecuteCb = std::function<void(const DynamicMessage&, DynamicActionServer&)>;

  DynamicActionServer(const std::string& ns, const ActionType& act, ExecuteCb cb)
      : ns_(ns[0] == '/' ? ns : "/" + ns),
        act_(act),
        execute_(std::move(cb)),
        status_pub_(ns_ + "/status"),
        result_pub_(ns_ + "/result", act.action_result()),
        feedback_pub_(ns_ + "/feedback", act.action_feedback()),
        goal_sub_(ns_ + "/goal", act.action_goal(),
                  [this](const DynamicMessage& m) { on_goal(m); }),
        cancel_sub_(ns_ + "/cancel",
                    [this](const actionlib_msgs::GoalID& m) { on_cancel(m); }) {
    running_.store(true);
    status_thread_ = std::thread([this] { status_loop(); });
  }
  ~DynamicActionServer() {
    running_.store(false);
    if (status_thread_.joinable()) status_thread_.join();
  }

  /// Blank goal-shaped messages for the handler to fill in.
  DynamicMessage createFeedback() const { return act_.feedback().create(); }
  DynamicMessage createResult() const { return act_.result().create(); }

  bool isPreemptRequested() { std::lock_guard<std::mutex> lk(mu_); return preempt_; }

  void publishFeedback(const DynamicMessage& fb) {
    DynamicMessage m = act_.action_feedback().create();
    uint32_t s, n;
    detail::stamp_now(&s, &n);
    m.set("header.stamp", Time{s, n});
    {
      std::lock_guard<std::mutex> lk(mu_);
      m.set("status.goal_id.id", cur_id_);
      m.set("status.status", static_cast<uint8_t>(GoalStatusVals::ACTIVE));
    }
    m.set("feedback", fb);
    feedback_pub_.publish(m);
  }

  void setSucceeded() { finish(GoalStatusVals::SUCCEEDED, act_.result().create()); }
  void setSucceeded(const DynamicMessage& r) { finish(GoalStatusVals::SUCCEEDED, r); }
  void setAborted() { finish(GoalStatusVals::ABORTED, act_.result().create()); }
  void setAborted(const DynamicMessage& r) { finish(GoalStatusVals::ABORTED, r); }
  void setPreempted() { finish(GoalStatusVals::PREEMPTED, act_.result().create()); }
  void setPreempted(const DynamicMessage& r) { finish(GoalStatusVals::PREEMPTED, r); }

 private:
  void status_loop() {
    Rate rate(10);
    while (running_.load() && ok()) { publish_status(); rate.sleep(); }
  }
  void publish_status() {
    actionlib_msgs::GoalStatusArray arr;
    detail::stamp_now(&arr.header.stamp_sec, &arr.header.stamp_nsec);
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (!cur_id_.empty()) {
        actionlib_msgs::GoalStatus st;
        st.goal_id.id = cur_id_;
        st.status = cur_status_;
        arr.status_list.push_back(st);
      }
    }
    status_pub_.publish(arr);
  }
  void on_goal(const DynamicMessage& ag) {
    std::string id = ag.get<std::string>("goal_id.id");
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (id == cur_id_) return;      // duplicate resend
      cur_id_ = id;
      cur_status_ = GoalStatusVals::ACTIVE;
      preempt_ = false;
    }
    DynamicMessage goal = ag.msg("goal");
    std::thread([this, goal] {
      execute_(goal, *this);
      std::lock_guard<std::mutex> lk(mu_);
      if (cur_status_ == GoalStatusVals::ACTIVE) cur_status_ = GoalStatusVals::ABORTED;
    }).detach();
  }
  void on_cancel(const actionlib_msgs::GoalID& g) {
    std::lock_guard<std::mutex> lk(mu_);
    if (g.id == cur_id_ || g.id.empty()) preempt_ = true;
  }
  void finish(uint8_t status, const DynamicMessage& r) {
    DynamicMessage m = act_.action_result().create();
    uint32_t s, n;
    detail::stamp_now(&s, &n);
    m.set("header.stamp", Time{s, n});
    {
      std::lock_guard<std::mutex> lk(mu_);
      m.set("status.goal_id.id", cur_id_);
      m.set("status.status", status);
      cur_status_ = status;
    }
    m.set("result", r);
    result_pub_.publish(m);
    publish_status();
  }

  std::string ns_;
  ActionType act_;
  ExecuteCb execute_;
  Publisher<actionlib_msgs::GoalStatusArray> status_pub_;
  DynamicPublisher result_pub_, feedback_pub_;
  DynamicSubscriber goal_sub_;
  Subscriber<actionlib_msgs::GoalID> cancel_sub_;
  std::mutex mu_;
  std::string cur_id_;
  uint8_t cur_status_ = GoalStatusVals::PENDING;
  bool preempt_ = false;
  std::atomic<bool> running_{false};
  std::thread status_thread_;
};

}  // namespace irap_noroslib
