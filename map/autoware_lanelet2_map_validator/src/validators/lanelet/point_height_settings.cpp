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

#include "lanelet2_map_validator/validators/lanelet/point_height_settings.hpp"

#include "lanelet2_map_validator/utils.hpp"

namespace lanelet::autoware::validation
{
namespace
{
lanelet::validation::RegisterMapValidator<PointHeightValidator> reg;
}

lanelet::validation::Issues PointHeightValidator::operator()(const lanelet::LaneletMap & map)
{
  lanelet::validation::Issues issues;

  lanelet::autoware::validation::appendIssues(issues, check_point_height_settings(map));

  return issues;
}

lanelet::validation::Issues PointHeightValidator::check_point_height_settings(
  const lanelet::LaneletMap & map)
{
  lanelet::validation::Issues issues;

  (void)map;

  return issues;
}
}  // namespace lanelet::autoware::validation
