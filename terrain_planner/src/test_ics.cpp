/****************************************************************************
 *
 *   Copyright (c) 2022 Jaeyoung Lim, All rights reserved.
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
 * 3. Neither the name terrain-navigation nor the names of its contributors may be
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
 * @brief Node to test planner in the view utiltiy map
 *
 *
 * @author Jaeyoung Lim <jalim@ethz.ch>
 */

#include "terrain_planner/maneuver_library.h"

#include <terrain_navigation/terrain_map.h>

#include <grid_map_msgs/GridMap.h>
#include <grid_map_ros/GridMapRosConverter.hpp>
#include "adaptive_viewutility/data_logger.h"

void addErrorLayer(const std::string layer_name, const std::string query_layer, const std::string reference_layer,
                   grid_map::GridMap& map) {
  map.add(layer_name);

  for (grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator) {
    const grid_map::Index index = *iterator;
    map.at(layer_name, index) = double(map.at(query_layer, index) - map.at(reference_layer, index));
  }
}

void calculateCircleICS(const std::string layer_name, std::shared_ptr<TerrainMap>& terrain_map, double radius) {
  /// Choose yaw state to calculate ICS state
  grid_map::GridMap& map = terrain_map->getGridMap();

  /// TODO: Get positon from gridmap
  terrain_map->AddLayerHorizontalDistanceTransform(radius, "ics_+", "distance_surface");
  terrain_map->AddLayerHorizontalDistanceTransform(-radius, "ics_-", "max_elevation");
  addErrorLayer(layer_name, "ics_-", "ics_+", terrain_map->getGridMap());
}

bool checkCollision(grid_map::GridMap& map, const Eigen::Vector2d pos_2d, const double yaw, const double yaw_rate) {
  auto manuever_library = std::make_shared<ManeuverLibrary>();

  double upper_altitude = std::numeric_limits<double>::infinity();
  double lower_altitude = -std::numeric_limits<double>::infinity();

  Eigen::Vector3d rate = Eigen::Vector3d(0.0, 0.0, yaw_rate);
  Eigen::Vector3d vel = Eigen::Vector3d(std::cos(yaw), std::sin(yaw), 0.0);
  double horizon = 2 * M_PI / std::abs(rate.z());
  Eigen::Vector3d pos = Eigen::Vector3d(pos_2d.x(), pos_2d.y(), 0.0);

  Trajectory trajectory = manuever_library->generateArcTrajectory(rate, horizon, pos, vel);
  for (auto& position : trajectory.position()) {
    if (!map.isInside(Eigen::Vector2d(position.x(), position.y()))) {
      // Handle outside states as part of collision surface
      // Consider it as a collision when the circle trajectory is outside of the map
      return true;
    };
    double min_collisionaltitude = map.atPosition("distance_surface", Eigen::Vector2d(position.x(), position.y()));
    if (min_collisionaltitude > lower_altitude) lower_altitude = min_collisionaltitude;
    double max_collisionaltitude = map.atPosition("max_elevation", Eigen::Vector2d(position.x(), position.y()));
    if (max_collisionaltitude < upper_altitude) upper_altitude = max_collisionaltitude;
  }

  if (upper_altitude > lower_altitude) {
    return false;
  } else {
    return true;
  }
}

void calculateYawICS(const std::string layer_name, grid_map::GridMap& map, const double yaw, const double yaw_rate) {
  /// Choose yaw state to calculate ICS state

  auto manuever_library = std::make_shared<ManeuverLibrary>();

  map.add(layer_name);

  for (grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator) {
    const grid_map::Index index = *iterator;
    Eigen::Vector2d pos_2d;
    map.getPosition(index, pos_2d);

    bool right_hand_circle_in_collision = checkCollision(map, pos_2d, yaw, yaw_rate);
    bool left_hand_circle_in_collision = checkCollision(map, pos_2d, yaw, -yaw_rate);
    map.at(layer_name, index) = (!right_hand_circle_in_collision || !left_hand_circle_in_collision);
  }
}

double getCoverage(const std::string layer_name, const double threshold, const grid_map::GridMap& map) {
  int cell_count{0};
  int valid_count{0};
  for (grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator) {
    const grid_map::Index index = *iterator;
    Eigen::Vector2d pos_2d;
    if (map.at(layer_name, index) > threshold) {
      valid_count++;
    }
    cell_count++;
  }

  double coverage = double(valid_count) / double(cell_count);
  return coverage;
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "terrain_planner");
  ros::NodeHandle nh("");
  ros::NodeHandle nh_private("~");

  ros::Publisher grid_map_pub_ = nh.advertise<grid_map_msgs::GridMap>("grid_map", 1, true);
  ros::Publisher yaw_pub_ = nh.advertise<visaulization_msgs::GridMap>("grid_map", 1, true);

  std::string map_path, map_color_path, output_file_path;
  bool visualize{true};
  nh_private.param<std::string>("map_path", map_path, "resources/cadastre.tif");
  nh_private.param<std::string>("color_file_path", map_color_path, "resources/cadastre.tif");
  nh_private.param<std::string>("output_file_path", output_file_path, "resources/output.csv");
  nh_private.param<bool>("visualize", visualize, true);
  std::shared_ptr<TerrainMap> terrain_map = std::make_shared<TerrainMap>();
  terrain_map->Load(map_path, false, map_color_path);
  terrain_map->AddLayerDistanceTransform(50.0, "distance_surface");
  terrain_map->AddLayerOffset(150.0, "max_elevation");

  std::cout << "Valid yaw terminal state coverage" << std::endl;
  auto data_logger = std::make_shared<DataLogger>();
  data_logger->setKeys({"yaw", "yaw_coverage", "circle_coverage"});
  std::cout << "Valid circlular terminal state coverage" << std::endl;
  calculateCircleICS("circle_error", terrain_map, 60.0);
  double circle_coverage = getCoverage("circle_error", 0.0, terrain_map->getGridMap());
  std::cout << "  - coverage: " << circle_coverage << std::endl;

  for (double yaw = 0.0; yaw < 2 * M_PI; yaw += 0.125 * M_PI) {
    calculateYawICS("yaw_error", terrain_map->getGridMap(), yaw, 0.25);
    double coverage = getCoverage("yaw_error", 0.0, terrain_map->getGridMap());
    std::cout << "  - yaw: " << yaw << std::endl;
    std::cout << "  - coverage: " << coverage << std::endl;
    std::unordered_map<std::string, std::any> state;
    state.insert(std::pair<std::string, double>("yaw", yaw));
    state.insert(std::pair<std::string, double>("yaw_coverage", coverage));
    state.insert(std::pair<std::string, double>("circle_coverage", circle_coverage));
    data_logger->record(state);
    grid_map_msgs::GridMap message;
    grid_map::GridMapRosConverter::toMessage(terrain_map->getGridMap(), message);
    grid_map_pub_.publish(message);
  }

  data_logger->setPrintHeader(true);
  data_logger->writeToFile(output_file_path);
  if (visualize) {
    while (true) {
      // Visualize yaw direction
      grid_map_msgs::GridMap message;
      grid_map::GridMapRosConverter::toMessage(terrain_map->getGridMap(), message);
      grid_map_pub_.publish(message);
      ///TODO: Publish arrow()

    }
  }
  ros::spin();
  return 0;
}
