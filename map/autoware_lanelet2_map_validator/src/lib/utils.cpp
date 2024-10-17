// Copyright 2023 Autoware Foundation
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

#include "lib/utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace lanelet
{
namespace autoware
{
namespace validation
{
lanelet::Point2d get_linestring_midpoint_2d(const ConstLineString3d & linestring)
{
  if (linestring.size() < 2) {
    std::ostringstream error_msg;
    error_msg << "LineString with ID " << linestring.id()
              << " must have at least two points to calculate the midpoint.";
    throw std::runtime_error(error_msg.str());
  }

  ConstPoint3d first_point = linestring.front();
  ConstPoint3d last_point = linestring.back();

  double mid_x = (first_point.x() + last_point.x()) / 2.0;
  double mid_y = (first_point.y() + last_point.y()) / 2.0;

  return lanelet::Point2d(0, mid_x, mid_y);  // 0 for dummy id
}

}  // namespace validation

}  // namespace autoware
}  // namespace lanelet
