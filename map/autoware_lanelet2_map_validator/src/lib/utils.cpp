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

#include "lib/utils.hpp"

#include <Eigen/Core>

namespace lanelet
{
namespace autoware
{
namespace validation
{

Eigen::Vector3d linestring_to_vector3d(const lanelet::ConstLineString3d linestring)
{
  Eigen::Vector3d front_point = linestring.front().basicPoint();
  Eigen::Vector3d back_point = linestring.back().basicPoint();

  return back_point - front_point;
}

}  // namespace validation
}  // namespace autoware
}  // namespace lanelet
