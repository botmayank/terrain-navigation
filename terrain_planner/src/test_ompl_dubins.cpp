/****************************************************************************
 *
 *   Copyright (c) 2021 Jaeyoung Lim, Autonomous Systems Lab,
 *  ETH Zürich. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @brief ROS Node to test ompl
 *
 *
 * @author Jaeyoung Lim <jalim@ethz.ch>
 */

#include <geometry_msgs/Point.h>
#include <ros/ros.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_eigen/tf2_eigen.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <visualization_msgs/MarkerArray.h>
#include <grid_map_ros/GridMapRosConverter.hpp>

#include "terrain_planner/common.h"
#include "terrain_planner/terrain_ompl_rrt.h"
#include "terrain_planner/visualization.h"

void getDubinsShortestPath(const Eigen::Vector3d start_pos, const double start_yaw, const Eigen::Vector3d goal_pos,
                           const double goal_yaw, std::vector<Eigen::Vector3d>& path) {
  auto dubins_ss = std::make_shared<fw_planning::spaces::DubinsAirplaneStateSpace>();

  ompl::base::State* from = dubins_ss->allocState();
  from->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setX(start_pos.x());
  from->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setY(start_pos.y());
  from->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setZ(start_pos.z());
  from->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setYaw(start_yaw);

  ompl::base::State* to = dubins_ss->allocState();
  to->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setX(goal_pos.x());
  to->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setY(goal_pos.y());
  to->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setZ(goal_pos.z());
  to->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->setYaw(goal_yaw);

  ompl::base::State* state = dubins_ss->allocState();
  bool publish = true;
  for (double t = 0.0; t < 1.0; t += 0.02) {
    dubins_ss->interpolate(from, to, t, state);
    auto interpolated_state =
        Eigen::Vector3d(state->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->getX(),
                        state->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->getY(),
                        state->as<fw_planning::spaces::DubinsAirplaneStateSpace::StateType>()->getZ());
    if (interpolated_state(0) >= std::numeric_limits<float>::max() && publish) {
      std::cout << "interpolated state had nans!" << std::endl;
      std::cout << "  - start_yaw: " << start_yaw << " goal_yaw: " << goal_yaw << std::endl;
      const double curvature = dubins_ss->getCurvature();
      const double dx = (goal_pos(0) - start_pos(0)) * curvature;
      const double dy = (goal_pos(1) - start_pos(1)) * curvature;
      const double dz = (goal_pos(2) - start_pos(2)) * curvature;
      const double fabs_dz = fabs(dz);
      const double th = atan2f(dy, dx);
      const double alpha = start_yaw - th;
      const double beta = goal_yaw - th;
      publish = false;
    }
    path.push_back(interpolated_state);
  }
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "ompl_rrt_planner");
  ros::NodeHandle nh("");
  ros::NodeHandle nh_private("~");

  // Initialize ROS related publishers for visualization
  auto start_pos_pub = nh.advertise<visualization_msgs::Marker>("start_position", 1, true);
  auto goal_pos_pub = nh.advertise<visualization_msgs::Marker>("goal_position", 1, true);
  auto path_pub = nh.advertise<nav_msgs::Path>("path", 1, true);
  auto trajectory_pub = nh.advertise<visualization_msgs::MarkerArray>("tree", 1, true);

  std::vector<double> start_yaw{0.0, 2.51681, 2.71681, 3.71681, 3.91681};
  std::vector<double> goal_yaw{3.53454, 6.17454, 6.23454, 0.25135, 0.31135};
  int idx = 0;
  while (true) {
    Eigen::Vector3d start_pos(0.0, 0.0, 0.0);
    Eigen::Vector3d goal_pos(152.15508, 0.0, 0.0);

    std::vector<Eigen::Vector3d> path;

    getDubinsShortestPath(start_pos, start_yaw[idx % start_yaw.size()], goal_pos, goal_yaw[idx % goal_yaw.size()],
                          path);

    publishTrajectory(path_pub, path);
    Eigen::Vector3d start_velocity(std::cos(start_yaw[idx % start_yaw.size()]),
                                   std::sin(start_yaw[idx % start_yaw.size()]), 0.0);
    publishPositionSetpoints(start_pos_pub, start_pos, start_velocity);
    Eigen::Vector3d goal_velocity(std::cos(goal_yaw[idx % goal_yaw.size()]), std::sin(goal_yaw[idx % goal_yaw.size()]),
                                  0.0);
    publishPositionSetpoints(goal_pos_pub, goal_pos, goal_velocity);
    ros::Duration(1.0).sleep();
    idx++;
  }
  ros::spin();
  return 0;
}
