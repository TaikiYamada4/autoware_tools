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

#include "common/cli.hpp"
#include "common/utils.hpp"
#include "common/validation.hpp"
#include "lanelet2_validation/Validation.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>

int main(int argc, char * argv[])
{
  lanelet::autoware::validation::MetaConfig meta_config =
    lanelet::autoware::validation::parseCommandLine(
      argc, const_cast<const char **>(argv));  // NOLINT

  // Print help (Already done in parseCommandLine)
  if (meta_config.command_line_config.help) {
    return 0;
  }

  // Print available validators
  if (meta_config.command_line_config.print) {
    auto checks = lanelet::validation::availabeChecks(  // cspell:disable-line
      meta_config.command_line_config.validationConfig.checksFilter);
    if (checks.empty()) {
      std::cout << "No checks found matching to '"
                << meta_config.command_line_config.validationConfig.checksFilter << "'"
                << std::endl;
    } else {
      std::cout << "The following checks are available:" << std::endl;
      for (auto & check : checks) {
        std::cout << check << std::endl;
      }
    }
    return 0;
  }

  // Validation start
  if (meta_config.command_line_config.mapFile.empty()) {
    throw std::invalid_argument("No map file specified!");
  } else if (!std::filesystem::is_regular_file(meta_config.command_line_config.mapFile)) {
    throw std::invalid_argument("Map file doesn't exist or is not a file!");
  }

  if (!meta_config.requirements_file.empty()) {
    if (!std::filesystem::is_regular_file(meta_config.requirements_file)) {
      throw std::invalid_argument("Input file doesn't exist or is not a file!");
    }
    std::ifstream input_file(meta_config.requirements_file);
    json json_data;
    input_file >> json_data;
    lanelet::autoware::validation::process_requirements(json_data, meta_config);
  } else {
    auto issues = lanelet::autoware::validation::validateMap(meta_config);
    lanelet::validation::printAllIssues(issues);
  }

  return 0;
}
