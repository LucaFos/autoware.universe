//
//  Copyright 2020 Tier IV, Inc. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#ifndef AUTOWARE_STATE_PANEL_HPP_
#define AUTOWARE_STATE_PANEL_HPP_

#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <rclcpp/rclcpp.hpp>
#include <rviz_common/panel.hpp>

#include <autoware_auto_system_msgs/msg/autoware_state.hpp>
#include <autoware_auto_vehicle_msgs/msg/engage.hpp>
#include <autoware_auto_vehicle_msgs/msg/gear_report.hpp>
#include <tier4_control_msgs/msg/external_command_selector_mode.hpp>
#include <tier4_control_msgs/msg/gate_mode.hpp>
#include <tier4_external_api_msgs/srv/engage.hpp>
#include <tier4_planning_msgs/msg/velocity_limit.hpp>

namespace rviz_plugins
{
class AutowareStatePanel : public rviz_common::Panel
{
  Q_OBJECT

public:
  explicit AutowareStatePanel(QWidget * parent = nullptr);
  void onInitialize() override;

public Q_SLOTS:
  void onClickAutowareEngage();
  void onClickVelocityLimit();

protected:
  void onGateMode(const tier4_control_msgs::msg::GateMode::ConstSharedPtr msg);
  void onSelectorMode(
    const tier4_control_msgs::msg::ExternalCommandSelectorMode::ConstSharedPtr msg);
  void onAutowareState(const autoware_auto_system_msgs::msg::AutowareState::ConstSharedPtr msg);
  void onShift(const autoware_auto_vehicle_msgs::msg::GearReport::ConstSharedPtr msg);
  void onEngageStatus(const autoware_auto_vehicle_msgs::msg::Engage::ConstSharedPtr msg);

  rclcpp::Node::SharedPtr raw_node_;
  rclcpp::Subscription<tier4_control_msgs::msg::GateMode>::SharedPtr sub_gate_mode_;
  rclcpp::Subscription<tier4_control_msgs::msg::ExternalCommandSelectorMode>::SharedPtr
    sub_selector_mode_;
  rclcpp::Subscription<autoware_auto_system_msgs::msg::AutowareState>::SharedPtr
    sub_autoware_state_;
  rclcpp::Subscription<autoware_auto_vehicle_msgs::msg::GearReport>::SharedPtr sub_gear_;
  rclcpp::Subscription<autoware_auto_vehicle_msgs::msg::Engage>::SharedPtr sub_engage_;

  rclcpp::Client<tier4_external_api_msgs::srv::Engage>::SharedPtr client_engage_;

  rclcpp::Publisher<tier4_planning_msgs::msg::VelocityLimit>::SharedPtr pub_velocity_limit_;

  QLabel * gate_mode_label_ptr_;
  QLabel * selector_mode_label_ptr_;
  QLabel * autoware_state_label_ptr_;
  QLabel * gear_label_ptr_;
  QLabel * engage_status_label_ptr_;
  QPushButton * engage_button_ptr_;
  QPushButton * velocity_limit_button_ptr_;
  QSpinBox * pub_velocity_limit_input_;

  bool current_engage_;
};

}  // namespace rviz_plugins

#endif  // AUTOWARE_STATE_PANEL_HPP_
