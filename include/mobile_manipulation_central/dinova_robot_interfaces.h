#pragma once

#include <ros/ros.h>
#include <Eigen/Eigen>

#include <geometry_msgs/Twist.h>
#include <sensor_msgs/JointState.h>
#include <std_msgs/Float64MultiArray.h>

namespace mm {

std::map<std::string, size_t> JOINT_INDEX_MAP = {
    {"joint_1", 0}, {"joint_2", 1},
    {"joint_3", 2}, {"joint_4", 3},
    {"joint_5", 4}, {"joint_6", 5}};

// TODO make a corresponding Vicon object class

class RobotROSInterface {
   public:
    RobotROSInterface(size_t nq, size_t nv) : nq_(nq), nv_(nv) {
        q_ = Eigen::VectorXd::Zero(nq);
        v_ = Eigen::VectorXd::Zero(nv);
    }

    size_t nq() const { return nq_; };
    size_t nv() const { return nv_; };

    virtual Eigen::VectorXd q() const { return q_; }

    virtual Eigen::VectorXd v() const { return v_; }

    virtual void brake() { publish_cmd_vel(Eigen::VectorXd::Zero(nv_)); }

    virtual bool ready() const { return joint_states_received_; }

    virtual void publish_cmd_vel(const Eigen::VectorXd& cmd_vel,
                                 bool bodyframe = false) = 0;

   protected:
    size_t nq_;
    size_t nv_;

    Eigen::VectorXd q_;
    Eigen::VectorXd v_;

    bool joint_states_received_ = false;

    ros::Subscriber joint_state_sub_;
    ros::Publisher cmd_pub_;
};

class DingoROSInterface : public RobotROSInterface {
   public:
    DingoROSInterface(ros::NodeHandle& nh) : RobotROSInterface(3, 3) {
        joint_state_sub_ =
            nh.subscribe("dingo/joint_states", 1,
                         &DingoROSInterface::joint_state_cb, this);

        cmd_pub_ =
            nh.advertise<geometry_msgs::Twist>("dingo/cmd_vel", 1, true);
    }

    void publish_cmd_vel(const Eigen::VectorXd& cmd_vel,
                         bool bodyframe = false) override {
        // bodyframe indicates whether the supplied command is in the body
        // frame or world frame. The robot takes commands in the body frame, so
        // if the command is in the world it must be rotated.
        if (cmd_vel.rows() != nv_) {
            throw std::runtime_error("Dingo given cmd_vel of wrong shape.");
        }

        geometry_msgs::Twist msg;
        if (bodyframe) {
            msg.linear.x = cmd_vel(0);
            msg.linear.y = cmd_vel(1);
        } else /* world frame */ {
            // we have to rotate into the body frame
            Eigen::Rotation2Dd C_bw(-q_(2));
            Eigen::Vector2d xy = C_bw * cmd_vel.head(2);
            msg.linear.x = xy(0);
            msg.linear.y = xy(1);
        }
        msg.angular.z = cmd_vel(2);
        cmd_pub_.publish(msg);
    }

   private:
    void joint_state_cb(const sensor_msgs::JointState& msg) {
        for (int i = 0; i < msg.name.size(); ++i) {
            q_[i] = msg.position[i];
            v_[i] = msg.velocity[i];
        }
        joint_states_received_ = true;
    }
};

class KinovaROSInterface : public RobotROSInterface {
   public:
    KinovaROSInterface(ros::NodeHandle& nh) : RobotROSInterface(6, 6) {
        joint_state_sub_ = nh.subscribe(
            "kinova/joint_states", 1, &KinovaROSInterface::joint_state_cb, this);

        cmd_pub_ =
            nh.advertise<std_msgs::Float64MultiArray>("kinova/cmd_vel", 1, true);
    }

    void publish_cmd_vel(const Eigen::VectorXd& cmd_vel,
                         bool bodyframe = false) override {
        if (cmd_vel.rows() != nv_) {
            throw std::runtime_error("Kinova given cmd_vel of wrong shape.");
        }

        std_msgs::Float64MultiArray msg;
        msg.data = std::vector<double>(cmd_vel.data(),
                                       cmd_vel.data() + cmd_vel.rows());
        cmd_pub_.publish(msg);
    }

   private:
    void joint_state_cb(const sensor_msgs::JointState& msg) {
        for (int i = 0; i < msg.name.size(); ++i) {
            size_t j = JOINT_INDEX_MAP.at(msg.name[i]);
            q_[j] = msg.position[i];
            v_[j] = msg.velocity[i];
        }
        joint_states_received_ = true;
    }
};

class MobileManipulatorDinovaROSInterface : public RobotROSInterface {
   public:
    MobileManipulatorDinovaROSInterface(ros::NodeHandle& nh)
        : base_(nh), arm_(nh), RobotROSInterface(9, 9) {}

    Eigen::VectorXd q() const override {
        Eigen::VectorXd q(nq_);
        q << base_.q(), arm_.q();
        return q;
    }

    Eigen::VectorXd v() const override {
        Eigen::VectorXd v(nv_);
        v << base_.v(), arm_.v();
        return v;
    }

    void brake() override {
        base_.brake();
        arm_.brake();
    }

    bool ready() const override { return base_.ready() && arm_.ready(); }

    void publish_cmd_vel(const Eigen::VectorXd& cmd_vel,
                         bool bodyframe = false) override {
        base_.publish_cmd_vel(cmd_vel.head(base_.nv()), bodyframe);
        arm_.publish_cmd_vel(cmd_vel.tail(arm_.nv()));
    }

   private:
    DingoROSInterface base_;
    KinovaROSInterface arm_;
};

}  // namespace mm
