/****************************************************************************
 *
 *   Copyright (c) 2021 Jaeyoung Lim. All rights reserved.
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
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * @brief Adaptive view utility estimation node
 *
 *
 * @author Jaeyoung Lim <jalim@ethz.ch>
 */

#ifndef TERRAIN_PLANNER_VIEWPOINT_H
#define TERRAIN_PLANNER_VIEWPOINT_H

#include <Eigen/Dense>
#include <iostream>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <vector>
#include "opencv2/core.hpp"

class ViewPoint {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  ViewPoint(const int idx, const Eigen::Vector3d &local_position, const Eigen::Vector4d &orientation) {
    index_ = idx;
    center_local_ = local_position;
    orientation_ = orientation;
    corner_ray_vectors_.push_back(RayVector(0, 0));
    corner_ray_vectors_.push_back(RayVector(0, 1080));
    corner_ray_vectors_.push_back(RayVector(720, 1080));
    corner_ray_vectors_.push_back(RayVector(720, 0));

    center_ray_vector_ = RayVector(360.0, 540);

    orientation_ = orientation;
    Eigen::Matrix3d R_att = quat2RotMatrix(orientation);

    // Transform corner ray vectors according to attitude
    for (auto &corner_ray : corner_ray_vectors_) {
      corner_ray = R_att * corner_ray;
    }
    center_ray_vector_ = R_att * center_ray_vector_;
  };
  ViewPoint(const int idx, const double &longitude, const double &latitude, const double &altitude) {
    index_ = idx;
    center_global << longitude, latitude, altitude;
    corner_ray_vectors_.push_back(RayVector(0, 0));
    corner_ray_vectors_.push_back(RayVector(0, 1080));
    corner_ray_vectors_.push_back(RayVector(720, 1080));
    corner_ray_vectors_.push_back(RayVector(720, 0));
  }
  virtual ~ViewPoint(){};
  void setOrigin(const double &latitude, const double &longitude, const double &altitude) {
    origin_global_ << latitude, longitude, altitude;
  };
  Eigen::Vector3d RayVector(int pixel_x, int pixel_y) {
    /// TODO: Hardcoded camera parameters
    /// TODO: Get camera intrinsics
    int c1 = 360.0;
    int c2 = 540.0;
    double f = 540.0;

    Eigen::Vector3d ray;
    ray << (pixel_x - c1) / f, (pixel_y - c2) / f, -1.0;
    ray.normalize();
    return ray;
  }
  Eigen::Vector3d getCenterLocal() { return center_local_; }
  double getTime() { return time_seconds_; }
  void setTime(double time_seconds) { time_seconds_ = time_seconds; }
  void setPosition(const Eigen::Vector3d &position) { center_local_ = position; }
  void setOrientation(const Eigen::Vector4d &attitude) {
    orientation_ = attitude;
    Eigen::Matrix3d R_att = quat2RotMatrix(attitude);

    // Transform corner ray vectors according to attitude
    for (auto &corner_ray : corner_ray_vectors_) {
      corner_ray = R_att * corner_ray;
    }
    center_ray_vector_ = R_att * center_ray_vector_;
  }
  void setUtility(const double &utility) { utility_ = utility; };
  void setImage(const std::string &image_path) {
    image_ = cv::imread(image_path, cv::IMREAD_GRAYSCALE);
    // cv::imshow("viewpoint", image_);
    // cv::waitKey(0.5);
  };
  cv::Mat getImage() { return image_; }
  std::vector<Eigen::Vector3d> getCornerRayVectors() { return corner_ray_vectors_; };
  Eigen::Vector3d getCenterRayVector() { return center_ray_vector_; };
  Eigen::Vector4d getOrientation() { return orientation_; };
  double getUtility() { return utility_; };
  int getIndex() { return index_; }
  static Eigen::Matrix3d quat2RotMatrix(const Eigen::Vector4d &q) {
    Eigen::Matrix3d rotmat;
    rotmat << q(0) * q(0) + q(1) * q(1) - q(2) * q(2) - q(3) * q(3), 2 * q(1) * q(2) - 2 * q(0) * q(3),
        2 * q(0) * q(2) + 2 * q(1) * q(3),

        2 * q(0) * q(3) + 2 * q(1) * q(2), q(0) * q(0) - q(1) * q(1) + q(2) * q(2) - q(3) * q(3),
        2 * q(2) * q(3) - 2 * q(0) * q(1),

        2 * q(1) * q(3) - 2 * q(0) * q(2), 2 * q(0) * q(1) + 2 * q(2) * q(3),
        q(0) * q(0) - q(1) * q(1) - q(2) * q(2) + q(3) * q(3);
    return rotmat;
  };

 private:
  int index_;
  Eigen::Vector3d center_local_{Eigen::Vector3d(0.0, 0.0, 0.0)};
  Eigen::Vector3d center_global{Eigen::Vector3d(0.0, 0.0, 0.0)};
  Eigen::Vector3d origin_global_{Eigen::Vector3d(0.0, 0.0, 0.0)};
  Eigen::Vector4d orientation_{Eigen::Vector4d(1.0, 0.0, 0.0, 0.0)};
  std::vector<Eigen::Vector3d> corner_ray_vectors_;
  Eigen::Vector3d center_ray_vector_;
  double time_seconds_{0.0};
  double utility_{0.0};
  cv::Mat image_;  // Store image of the viewpoint
};

#endif
