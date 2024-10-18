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

#include <Eigen/Core>

#include <lanelet2_core/LaneletMap.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
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

lanelet::LineString3d get_starting_edge_from_lanelet(
  const lanelet::ConstLanelet & lanelet, const lanelet::ConstLineString3d & reference)
{
  Eigen::Vector3d front_l = lanelet.leftBound().front().basicPoint();
  Eigen::Vector3d back_l = lanelet.leftBound().back().basicPoint();

  Eigen::Vector3d front_r = lanelet.rightBound().front().basicPoint();
  Eigen::Vector3d back_r = lanelet.rightBound().back().basicPoint();

  Eigen::Vector3d ref_1 = reference.front().basicPoint();
  Eigen::Vector3d ref_2 = reference.back().basicPoint();

  auto norm_sum = [ref_1, ref_2](const Eigen::Vector3d & vec1, const Eigen::Vector3d & vec2) {
    return (vec1 - ref_1).norm() + (vec2 - ref_2).norm();
  };

  double front_min = std::min(norm_sum(front_l, front_r), norm_sum(front_r, front_l));
  double back_min = std::min(norm_sum(back_l, back_r), norm_sum(back_r, back_l));

  Points3d points;
  if (front_min <= back_min) {
    points.push_back(Point3d(front_l.x(), front_l.y(), front_l.z()));
    points.push_back(Point3d(front_r.x(), front_r.y(), front_r.z()));
  } else {
    points.push_back(Point3d(back_l.x(), back_l.y(), back_l.z()));
    points.push_back(Point3d(back_r.x(), back_r.y(), back_r.z()));
  }

  return LineString3d(reference.id(), points);  // same id for debug reasons
}

lanelet::validation::Issues TrafficLightFacingValidator::check_traffic_light_facing(
  const lanelet::LaneletMap & map)
{
  lanelet::validation::Issues issues;

  // Get all red_yellow_green traffic lights with a map of flags
  std::map<lanelet::Id, bool> tl_has_been_judged_correct;
  std::map<lanelet::Id, bool> tl_has_been_judged_wrong;

  for (const lanelet::ConstLineString3d & linestring : map.lineStringLayer) {
    if (is_red_yellow_green_traffic_light(linestring)) {
      std::cout << "id: " << linestring.id() << std::endl;
      tl_has_been_judged_correct.insert({linestring.id(), false});
      tl_has_been_judged_wrong.insert({linestring.id(), false});
    }
  }

  //
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

  std::cout << "tl_ids.size() = " << tl_has_been_judged_correct.size() << std::endl;

  // Digest the stop line non-existance and the traffic light facing error to issues
  for (const auto & entry : tl_has_been_judged_correct) {
    lanelet::Id traffic_light_id = entry.first;
    bool has_stop_line = entry.second;

    if (has_stop_line == false) {
      issues.emplace_back(
        lanelet::validation::Severity::Error, lanelet::validation::Primitive::LineString,
        traffic_light_id, "Cannot find a corresponding stop line to this traffic light.");
    } else if (tl_has_been_judged_wrong.at(traffic_light_id) == false) {
      issues.emplace_back(
        lanelet::validation::Severity::Error, lanelet::validation::Primitive::LineString,
        traffic_light_id, "The traffic light facing seems to be opposite.");
    }
  }

  return issues;
}

}  // namespace validation
}  // namespace lanelet
