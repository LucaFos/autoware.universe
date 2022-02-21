// Copyright 2021 Tier IV, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <scene_module/occlusion_spot/manager.hpp>
#include <scene_module/occlusion_spot/scene_occlusion_spot_in_private_road.hpp>
#include <scene_module/occlusion_spot/scene_occlusion_spot_in_public_road.hpp>
#include <utilization/util.hpp>

#include <lanelet2_core/primitives/BasicRegulatoryElements.h>

#include <memory>
#include <string>
#include <vector>

namespace behavior_velocity_planner
{
namespace
{
std::vector<lanelet::ConstLanelet> getLaneletsOnPath(
  const autoware_auto_planning_msgs::msg::PathWithLaneId & path,
  const lanelet::LaneletMapPtr lanelet_map)
{
  std::vector<lanelet::ConstLanelet> lanelets;

  for (const auto & p : path.points) {
    const auto lane_id = p.lane_ids.at(0);
    lanelets.push_back(lanelet_map->laneletLayer.get(lane_id));
  }
  return lanelets;
}

bool hasPublicRoadOnPath(
  const autoware_auto_planning_msgs::msg::PathWithLaneId & path,
  const lanelet::LaneletMapPtr lanelet_map)
{
  for (const auto & ll : getLaneletsOnPath(path, lanelet_map)) {
    // Is public load ?
    const std::string location = ll.attributeOr("location", "else");
    if (location == "urban" || location == "public") {
      return true;
    }
  }
  return false;
}

bool hasPrivateRoadOnPath(
  const autoware_auto_planning_msgs::msg::PathWithLaneId & path,
  const lanelet::LaneletMapPtr lanelet_map)
{
  for (const auto & ll : getLaneletsOnPath(path, lanelet_map)) {
    // Is private load ?
    const std::string location = ll.attributeOr("location", "else");
    if (location == "private") {
      return true;
    }
  }
  return false;
}

}  // namespace

OcclusionSpotModuleManager::OcclusionSpotModuleManager(rclcpp::Node & node)
: SceneModuleManagerInterface(node, getModuleName())
{
  const std::string ns(getModuleName());
  pub_debug_occupancy_grid_ =
    node.create_publisher<nav_msgs::msg::OccupancyGrid>("~/debug/" + ns + "/occupancy_grid", 1);

  // for crosswalk parameters
  auto & pp = planner_param_;
  // assume pedestrian coming out from occlusion spot with this velocity
  pp.debug = node.declare_parameter(ns + ".debug", false);
  pp.pedestrian_vel = node.declare_parameter(ns + ".pedestrian_vel", 1.0);
  pp.detection_area_length = node.declare_parameter(ns + ".threshold.detection_area_length", 200.0);
  pp.stuck_vehicle_vel = node.declare_parameter(ns + ".threshold.stuck_vehicle_vel", 1.0);
  pp.lateral_distance_thr = node.declare_parameter(ns + ".threshold.lateral_distance", 10.0);

  pp.dist_thr = node.declare_parameter(ns + ".threshold.search_dist", 10.0);
  pp.angle_thr = node.declare_parameter(ns + ".threshold.search_angle", M_PI / 5.0);

  // ego additional velocity config
  pp.v.safety_ratio = node.declare_parameter(ns + ".motion.safety_ratio", 1.0);
  pp.v.delay_time = node.declare_parameter(ns + ".motion.delay_time", 0.1);
  pp.v.safe_margin = node.declare_parameter(ns + ".motion.safe_margin", 1.0);
  pp.v.max_slow_down_jerk = node.declare_parameter(ns + ".motion.max_slow_down_jerk", -0.7);
  pp.v.max_slow_down_accel = node.declare_parameter(ns + ".motion.max_slow_down_accel", -2.5);
  pp.v.non_effective_jerk = node.declare_parameter(ns + ".motion.non_effective_jerk", -0.3);
  pp.v.non_effective_accel =
    node.declare_parameter(ns + ".motion.non_effective_acceleration", -1.0);
  pp.v.min_allowed_velocity = node.declare_parameter(ns + ".motion.min_allowed_velocity", 1.0);
  // detection_area param
  pp.detection_area.min_occlusion_spot_size =
    node.declare_parameter(ns + ".detection_area.min_occlusion_spot_size", 2.0);
  pp.detection_area.max_lateral_distance =
    node.declare_parameter(ns + ".detection_area.max_lateral_distance", 4.0);
  pp.detection_area.slice_length = node.declare_parameter(ns + ".detection_area.slice_length", 1.5);
  // occupancy grid param
  pp.grid.free_space_max = node.declare_parameter(ns + ".grid.free_space_max", 10);
  pp.grid.occupied_min = node.declare_parameter(ns + ".grid.occupied_min", 51);

  const auto vehicle_info = vehicle_info_util::VehicleInfoUtil(node).getVehicleInfo();
  pp.baselink_to_front = vehicle_info.max_longitudinal_offset_m;
  pp.half_vehicle_width = 0.5 * vehicle_info.vehicle_width_m;
}

void OcclusionSpotModuleManager::launchNewModules(
  const autoware_auto_planning_msgs::msg::PathWithLaneId & path)
{
  const int64_t private_road_module_id = static_cast<int64_t>(ModuleID::PRIVATE);
  const int64_t public_road_module_id = static_cast<int64_t>(ModuleID::PUBLIC);
  // private
  if (!isModuleRegistered(private_road_module_id)) {
    if (hasPrivateRoadOnPath(path, planner_data_->lanelet_map)) {
      registerModule(std::make_shared<OcclusionSpotInPrivateModule>(
        private_road_module_id, planner_data_, planner_param_,
        logger_.get_child("occlusion_spot_in_private_module"), clock_, pub_debug_occupancy_grid_));
    }
  }
  // public
  if (!isModuleRegistered(public_road_module_id)) {
    if (hasPublicRoadOnPath(path, planner_data_->lanelet_map)) {
      registerModule(std::make_shared<OcclusionSpotInPublicModule>(
        public_road_module_id, planner_data_, planner_param_,
        logger_.get_child("occlusion_spot_in_public_module"), clock_));
    }
  }
}

std::function<bool(const std::shared_ptr<SceneModuleInterface> &)>
OcclusionSpotModuleManager::getModuleExpiredFunction(
  const autoware_auto_planning_msgs::msg::PathWithLaneId & path)
{
  const bool has_public_road = hasPublicRoadOnPath(path, planner_data_->lanelet_map);
  const bool has_private_road = hasPrivateRoadOnPath(path, planner_data_->lanelet_map);
  return [has_private_road,
          has_public_road](const std::shared_ptr<SceneModuleInterface> & scene_module) {
    if (scene_module->getModuleId() == static_cast<int64_t>(ModuleID::PRIVATE)) {
      return !has_private_road;
    }
    if (scene_module->getModuleId() == static_cast<int64_t>(ModuleID::PUBLIC)) {
      return !has_public_road;
    }
    return true;
  };
}
}  // namespace behavior_velocity_planner