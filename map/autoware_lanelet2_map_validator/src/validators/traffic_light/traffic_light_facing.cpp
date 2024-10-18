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

lanelet::validation::Issues TrafficLightFacingValidator::check_traffic_light_facing(
  const lanelet::LaneletMap & map)
{
  lanelet::validation::Issues issues;

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
