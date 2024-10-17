// Copyright 2024 Autoware Foundation
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

#include "validators/traffic_light/traffic_light_facing.hpp"

#include "lib/utils.hpp"

#include <lanelet2_core/LaneletMap.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace lanelet
{
namespace validation
{
namespace
{
lanelet::validation::RegisterMapValidator<TrafficLightFacingValidator> reg;
}  // namespace

lanelet::validation::Issues TrafficLightFacingValidator::operator()(const lanelet::LaneletMap & map)
{
  lanelet::validation::Issues issues;

  // Remove this line and write down how to append issues
  lanelet::autoware::validation::appendIssues(issues, check_traffic_light_facing(map));

  return issues;
}

lanelet::ConstLineString3d TrafficLightFacingValidator::get_stop_line_from_reg_elem(
  const lanelet::RegulatoryElementConstPtr & reg_elem)
{
  if (!reg_elem) {
    throw std::invalid_argument("reg_elem is a null pointer.");
  }

  // Assume that there is only one stop_line
  for (const lanelet::ConstLineString3d & ref_line :
       reg_elem->getParameters<lanelet::ConstLineString3d>(lanelet::RoleName::RefLine)) {
    if (
      ref_line.hasAttribute(lanelet::AttributeName::Type) &&
      ref_line.attribute(lanelet::AttributeName::Type).value() ==
        lanelet::AttributeValueString::StopLine) {
      std::cout << "stop_line ID: " << ref_line.id() << std::endl;
      return ref_line;
    }
  }

  // If there is not stop_line return an empty one.
  return lanelet::ConstLineString3d();
}

bool TrafficLightFacingValidator::is_red_yellow_green_traffic_light(
  const lanelet::ConstLineString3d & linestring)
{
  return linestring.hasAttribute(lanelet::AttributeName::Type) &&
         linestring.hasAttribute(lanelet::AttributeName::Subtype) &&
         linestring.attribute(lanelet::AttributeName::Type).value() ==
           lanelet::AttributeValueString::TrafficLight &&
         linestring.attribute(lanelet::AttributeName::Subtype).value() ==
           lanelet::AttributeValueString::RedYellowGreen;
}

double TrafficLightFacingValidator::sine_of_angle(
  const lanelet::ConstLineString3d & traffic_light, const lanelet::ConstLineString3d & stop_line)
{
  if (traffic_light.size() < 2) {
    std::ostringstream error_msg;
    error_msg << "LineString with ID " << traffic_light.id()
              << " must have at least two points to calculate the midpoint.";
    throw std::runtime_error(error_msg.str());
  }

  if (stop_line.size() < 2) {
    std::ostringstream error_msg;
    error_msg << "LineString with ID " << stop_line.id()
              << " must have at least two points to calculate the midpoint.";
    throw std::runtime_error(error_msg.str());
  }

  lanelet::Point2d tl_midpoint =
    lanelet::autoware::validation::get_linestring_midpoint_2d(traffic_light);
  lanelet::Point2d sl_midpoint =
    lanelet::autoware::validation::get_linestring_midpoint_2d(stop_line);

  std::cout << std::setprecision(8) << "traffic_light: (" << traffic_light.front().x() << ", "
            << traffic_light.front().y() << ") -> (" << traffic_light.back().x() << ", "
            << traffic_light.back().y() << ")" << std::endl;

  std::cout << std::setprecision(8) << "tl_midpoint: (" << tl_midpoint.x() << ", "
            << tl_midpoint.y() << ")" << std::endl;

  std::cout << std::setprecision(8) << "stop_line: (" << stop_line.front().x() << ", "
            << stop_line.front().y() << ") -> (" << stop_line.back().x() << ", "
            << stop_line.back().y() << ")" << std::endl;

  std::cout << std::setprecision(8) << "sl_midpoint: (" << sl_midpoint.x() << ", "
            << sl_midpoint.y() << ")" << std::endl;

  double a_x = traffic_light.back().x() - traffic_light.front().x();
  double a_y = traffic_light.back().y() - traffic_light.front().y();
  double b_x = tl_midpoint.x() - sl_midpoint.x();
  double b_y = tl_midpoint.y() - sl_midpoint.y();

  std::cout << std::setprecision(8) << "vec_a: (" << a_x << ", " << a_y << ")" << std::endl;
  std::cout << std::setprecision(8) << "vec_b: (" << b_x << ", " << b_y << ")" << std::endl;

  return (a_x * b_y - a_y * b_x) / (std::hypot(a_x, a_y) * std::hypot(b_x, b_y));
}

lanelet::validation::Issues TrafficLightFacingValidator::check_traffic_light_facing(
  const lanelet::LaneletMap & map)
{
  lanelet::validation::Issues issues;
  double sine_tolerance = std::sin(10 * M_PI / 180);

  // Get all red_yellow_green traffic lights with a map of flags
  std::map<lanelet::Id, bool> tl_has_corresponding_stop_line;
  std::map<lanelet::Id, bool> tl_is_facing_correct;

  for (const lanelet::ConstLineString3d & linestring : map.lineStringLayer) {
    if (is_red_yellow_green_traffic_light(linestring)) {
      std::cout << "id: " << linestring.id() << std::endl;
      tl_has_corresponding_stop_line.insert({linestring.id(), false});
      tl_is_facing_correct.insert({linestring.id(), false});
    }
  }

  // Get all regulatory_elements that refers to RedYellowGreen traffic_lights and also refers to
  // stop_lines Then, calculate the cross product between (stop_line -> traffic_light) and
  // (traffic_light_line_direction)
  for (const lanelet::RegulatoryElementConstPtr & reg_elem : map.regulatoryElementLayer) {
    if (
      reg_elem->attribute(lanelet::AttributeName::Subtype).value() !=
      lanelet::AttributeValueString::TrafficLight) {
      continue;
    }
    std::cout << "-------" << std::endl;
    std::cout << "Found traffic light regulatory element with ID: " << reg_elem->id() << std::endl;
    lanelet::ConstLineString3d stop_line = get_stop_line_from_reg_elem(reg_elem);

    for (const lanelet::ConstLineString3d & refers_linestring :
         reg_elem->getParameters<lanelet::ConstLineString3d>(lanelet::RoleName::Refers)) {
      if (!is_red_yellow_green_traffic_light(refers_linestring)) {
        continue;
      }
      std::cout << "traffic_light ID: " << refers_linestring.id() << std::endl;

      double sine = sine_of_angle(refers_linestring, stop_line);
      std::cout << "stop_line_id: " << stop_line.id()
                << ", traffic_light_id: " << refers_linestring.id() << " => "
                << std::setprecision(3) << sine << std::endl;
      if (std::abs(sine - 1) <= sine_tolerance) {
        // The traffic light (refers_linestring) facing seems to be wrong.
        tl_has_corresponding_stop_line[refers_linestring.id()] = true;
        tl_is_facing_correct[refers_linestring.id()] = true;
      } else if (std::abs(sine - (-1)) <= sine_tolerance) {
        // The traffic light (refers_linestring) facing seems to be wrong.
        // There is a slight chance that the spatial relation between the stop line and the traffic
        // light is wrong
        tl_has_corresponding_stop_line[refers_linestring.id()] = true;
        tl_is_facing_correct[refers_linestring.id()] = false;
      }
      // If the sine didn't satisfy the both above, the traffic_light doesn't directly relate to the
      // stop line
    }
  }

  std::cout << "tl_ids.size() = " << tl_has_corresponding_stop_line.size() << std::endl;

  // Digest the stop line non-existance and the traffic light facing error to issues
  for (const auto & entry : tl_has_corresponding_stop_line) {
    lanelet::Id traffic_light_id = entry.first;
    bool has_stop_line = entry.second;

    if (has_stop_line == false) {
      issues.emplace_back(
        lanelet::validation::Severity::Error, lanelet::validation::Primitive::LineString,
        traffic_light_id, "Cannot find a corresponding stop line to this traffic light.");
    } else if (tl_is_facing_correct.at(traffic_light_id) == false) {
      issues.emplace_back(
        lanelet::validation::Severity::Error, lanelet::validation::Primitive::LineString,
        traffic_light_id, "The traffic light facing seems to be opposite.");
    }
  }

  return issues;
}

}  // namespace validation
}  // namespace lanelet
