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

#include "lanelet2_validation/Validation.h"
#include "lib/cli.hpp"
#include "lib/utils.hpp"
#include "lib/validation.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iomanip>
#include <queue>

// Use nlohmann::json for JSON handling
using json = nlohmann::json;

// ANSI color codes for console output
#define BOLD_ONLY "\033[1m"
#define BOLD_GREEN "\033[1;32m"
#define BOLD_YELLOW "\033[1;33m"
#define BOLD_RED "\033[1;31m"
#define NORMAL_GREEN "\033[32m"
#define NORMAL_RED "\033[31m"
#define FONT_RESET "\033[0m"

struct ValidatorInfo
{
  enum class Severity { ERROR, WARNING, INFO, NONE };

  std::vector<std::string> prerequisites;
  std::unordered_map<std::string, bool> forgive_warnings;
  Severity max_severity = Severity::NONE;
};

using Validators = std::unordered_map<std::string, ValidatorInfo>;

Validators parse_validators(const json & json_data)
{
  Validators validators;

  for (const auto & requirement : json_data["requirements"]) {
    for (const auto & validator : requirement["validators"]) {
      ValidatorInfo info;

      if (validator.contains("prerequisites")) {
        for (const auto & prereq : validator["prerequisites"]) {
          info.prerequisites.push_back(prereq["name"]);
          if (prereq.contains("forgive_warnings")) {
            info.forgive_warnings[prereq["name"]] = prereq["forgive_warnings"];
          }
        }
      }
      validators[validator["name"]] = info;
    }
  }
  return validators;
}

std::pair<std::vector<std::string>, Validators> create_validation_queue(
  const Validators & validators)
{
  std::unordered_map<std::string, std::vector<std::string>> graph;  // Graph of dependencies
  std::unordered_map<std::string, int>
    indegree;  // Indegree (number of prerequisites) for each validator
  std::vector<std::string> validation_queue;
  Validators remaining_validators;  // Validators left unprocessed

  // Build the graph and initialize indegree
  for (const auto & v : validators) {
    std::string name = v.first;
    indegree[name] = 0;              // Initialize indegree
    remaining_validators.insert(v);  // Throw all validators to remaining_validators first

    for (const auto & prereq : v.second.prerequisites) {
      graph[prereq].push_back(name);  // prereq -> validator (prereq points to validator)
      indegree[name]++;               // Increment the indegree of the validator
    }
  }

  // Use a queue to store validators with no prerequisites (indegree == 0)
  std::queue<std::string> q;
  for (const auto & [name, count] : indegree) {
    if (count == 0) {
      q.push(name);
      remaining_validators.erase(name);  // Remove from unprocessed set
    }
  }

  // Perform topological sort to derive execution order
  while (!q.empty()) {
    std::string current_validator_name = q.front();
    q.pop();

    // Add the current validator to the execution queue
    validation_queue.push_back(current_validator_name);

    // For each dependent validator, reduce indegree and add to the queue if indegree becomes 0
    for (const auto & neighbor : graph[current_validator_name]) {
      indegree[neighbor]--;
      if (indegree[neighbor] == 0) {
        q.push(neighbor);
        remaining_validators.erase(neighbor);  // Remove from unprocessed set
      }
    }
  }

  for (auto & [name, info] : remaining_validators) {
    info.max_severity = ValidatorInfo::Severity::ERROR;
  }

  return {validation_queue, remaining_validators};
}

// Function to find a validator block by name
json & find_validator_block(json & json_data, const std::string & validator_name)
{
  for (auto & requirement : json_data["requirements"]) {
    for (auto & validator : requirement["validators"]) {
      if (validator["name"] == validator_name) {
        return validator;  // Return a reference to the found validator
      }
    }
  }
  throw std::runtime_error("Validator not found");  // Handle case where validator is not found
}

std::vector<lanelet::validation::DetectedIssues> descript_unused_validators_to_json(
  json & json_data, const Validators & unused_validators)
{
  lanelet::validation::Issues issues;
  std::vector<lanelet::validation::DetectedIssues> detected_issues;

  for (const auto & [name, validator] : unused_validators) {
    json & validator_json = find_validator_block(json_data, name);
    validator_json["passed"] = false;
    json issues_json;
    json issue_json;
    issue_json["severity"] = lanelet::validation::toString(lanelet::validation::Severity::Error);
    issue_json["primitive"] =
      lanelet::validation::toString(lanelet::validation::Primitive::Primitive);
    issue_json["id"] = 0;
    issue_json["message"] = "Prerequisites don't exist OR they are making a loop.";
    issues_json.push_back(issue_json);
    validator_json["issues"] = issues_json;

    lanelet::validation::Issue issue;
    issue.severity = lanelet::validation::Severity::Error;
    issue.primitive = lanelet::validation::Primitive::Primitive;
    issue.id = lanelet::InvalId;
    issue.message = "Prerequisites don't exist OR they are making a loop.";
    issues.push_back(issue);
  }

  if (issues.size() > 0) {
    detected_issues.push_back({"invalid_prerequisites", issues});
  }
  return detected_issues;
}

std::vector<lanelet::validation::DetectedIssues> check_prerequisite_completion(
  json & json_data, const Validators & validators, const std::string target_validator_name)
{
  lanelet::validation::Issues issues;
  std::vector<lanelet::validation::DetectedIssues> detected_issues;

  ValidatorInfo current_validator_info = validators.at(target_validator_name);
  json & validator_json = find_validator_block(json_data, target_validator_name);

  bool prerequisite_complete = true;
  for (const auto & prereq : current_validator_info.prerequisites) {
    if (
      validators.at(prereq).max_severity == ValidatorInfo::Severity::ERROR ||
      (validators.at(prereq).max_severity == ValidatorInfo::Severity::WARNING &&
       !current_validator_info.forgive_warnings.at(prereq))) {
      prerequisite_complete = false;
      break;
    }
  }

  if (!prerequisite_complete) {
    validator_json["passed"] = false;
    json issues_json;
    json issue_json;
    issue_json["severity"] = lanelet::validation::toString(lanelet::validation::Severity::Error);
    issue_json["primitive"] =
      lanelet::validation::toString(lanelet::validation::Primitive::Primitive);
    issue_json["id"] = 0;
    issue_json["message"] = "Prerequisites didn't pass";
    issues_json.push_back(issue_json);
    validator_json["issues"] = issues_json;

    lanelet::validation::Issue issue;
    issue.severity = lanelet::validation::Severity::Error;
    issue.primitive = lanelet::validation::Primitive::Primitive;
    issue.id = lanelet::InvalId;
    issue.message = "Prerequisites didn't pass";
    issues.push_back(issue);
  }

  if (issues.size() > 0) {
    detected_issues.push_back({target_validator_name, issues});
  }

  return detected_issues;
}

uint64_t summarize_validator_results(json & json_data)
{
  uint64_t warning_count = 0;
  uint64_t error_count = 0;

  for (auto & requirement : json_data["requirements"]) {
    std::string id = requirement["id"];
    bool is_requirement_passed = true;
    std::map<std::string, bool> validator_results;

    for (const auto & validator : requirement["validators"]) {
      bool validator_passed = validator["passed"];
      validator_results[validator["name"]] = validator_passed;
      is_requirement_passed &= validator_passed;

      if (!validator.contains("issues")) {
        continue;
      }
      for (const auto & issue : validator["issues"]) {
        if (
          issue["severity"] ==
          lanelet::validation::toString(lanelet::validation::Severity::Warning)) {
          warning_count++;
        } else if (
          issue["severity"] ==
          lanelet::validation::toString(lanelet::validation::Severity::Error)) {
          error_count++;
        }
      }
    }

    std::cout << BOLD_ONLY << "[" << id << "] ";

    if (is_requirement_passed) {
      requirement["passed"] = true;
      std::cout << BOLD_GREEN << "Passed" << FONT_RESET << std::endl;
    } else {
      requirement["passed"] = false;
      std::cout << BOLD_RED << "Failed" << FONT_RESET << std::endl;
    }

    for (const auto & [name, result] : validator_results) {
      if (result) {
        std::cout << "  - " << name << ": " << NORMAL_GREEN << "Passed" << FONT_RESET << std::endl;
      } else {
        std::cout << "  - " << name << ": " << NORMAL_RED << "Failed" << FONT_RESET << std::endl;
      }
    }
  }

  uint64_t total_count = warning_count + error_count;
  if (total_count == 0) {
    std::cout << BOLD_GREEN << "No errors nor warnings were found" << FONT_RESET << std::endl;
  } else {
    if (warning_count > 0) {
      std::cout << BOLD_YELLOW << "Total of " << warning_count << " warnings were found"
                << FONT_RESET << std::endl;
    }
    if (error_count > 0) {
      std::cout << BOLD_RED << "Total of " << error_count << " errors were found" << FONT_RESET
                << std::endl;
    }
  }

  return total_count;
}

int new_process_requirements(
  json json_data, const lanelet::autoware::validation::MetaConfig & validator_config)
{
  std::vector<lanelet::validation::DetectedIssues> issues;
  lanelet::autoware::validation::MetaConfig temp_validator_config = validator_config;

  Validators validators = parse_validators(json_data);
  auto [validation_queue, remaining_validators] = create_validation_queue(validators);

  // temp print validation_queue
  for (const auto & validation_name : validation_queue) {
    std::cout << validation_name << std::endl;
  }
  std::vector<lanelet::validation::DetectedIssues> unused_validator_issues =
    descript_unused_validators_to_json(json_data, remaining_validators);
  lanelet::autoware::validation::appendIssues(issues, std::move(unused_validator_issues));
  for (const auto & validator_name : validation_queue) {
    temp_validator_config.command_line_config.validationConfig.checksFilter = validator_name;
    std::vector<lanelet::validation::DetectedIssues> prerequisite_failure_issues =
      check_prerequisite_completion(json_data, validators, validator_name);
    lanelet::autoware::validation::appendIssues(issues, std::move(prerequisite_failure_issues));
    if (prerequisite_failure_issues.size() > 0) {
      continue;
    }
    std::vector<lanelet::validation::DetectedIssues> temp_issues =
      lanelet::autoware::validation::validateMap(temp_validator_config);
    json & validator_json = find_validator_block(json_data, validator_name);

    if (temp_issues.size() == 0) {
      validator_json["passed"] = true;
      continue;
    }

    if (temp_issues[0].warnings().size() + temp_issues[0].errors().size() == 0) {
      validator_json["passed"] = true;
    } else {
      validator_json["passed"] = false;
    }
    if (temp_issues[0].issues.size() > 0) {
      json issues_json;
      for (const auto & issue : temp_issues[0].issues) {
        json issue_json;
        issue_json["severity"] = lanelet::validation::toString(issue.severity);
        issue_json["primitive"] = lanelet::validation::toString(issue.primitive);
        issue_json["id"] = issue.id;
        issue_json["message"] = issue.message;
        issues_json.push_back(issue_json);

        if (
          static_cast<int>(issue.severity) <
          static_cast<int>(validators[validator_name].max_severity)) {
          validators[validator_name].max_severity =
            static_cast<ValidatorInfo::Severity>(static_cast<int>(issue.severity));
        }
      }
      validator_json["issues"] = issues_json;
    }
    lanelet::autoware::validation::appendIssues(issues, std::move(temp_issues));
  }

  uint64_t num_issues = summarize_validator_results(json_data);
  lanelet::validation::printAllIssues(issues);

  if (!validator_config.output_file_path.empty()) {
    std::string file_name = validator_config.output_file_path + "/lanelet2_validation_results.json";
    std::ofstream output_file(file_name);
    output_file << std::setw(4) << json_data;
    std::cout << "Results are output to " << file_name << std::endl;
  }

  return (num_issues == 0) ? 0 : 1;
}

int process_requirements(
  json json_config, const lanelet::autoware::validation::MetaConfig & validator_config)
{
  uint64_t warning_count = 0;
  uint64_t error_count = 0;
  lanelet::autoware::validation::MetaConfig temp_validator_config = validator_config;

  for (auto & requirement : json_config["requirements"]) {
    std::string id = requirement["id"];
    bool requirement_passed = true;

    std::vector<lanelet::validation::DetectedIssues> issues;
    std::map<std::string, bool> temp_validation_results;

    for (auto & validator : requirement["validators"]) {
      std::string validator_name = validator["name"];
      temp_validator_config.command_line_config.validationConfig.checksFilter = validator_name;

      std::vector<lanelet::validation::DetectedIssues> temp_issues =
        lanelet::autoware::validation::validateMap(temp_validator_config);

      if (temp_issues.empty()) {
        // Validator passed
        temp_validation_results[validator_name] = true;
        validator["passed"] = true;
      } else {
        // Validator failed
        requirement_passed = false;
        warning_count += temp_issues[0].warnings().size();
        error_count += temp_issues[0].errors().size();
        temp_validation_results[validator_name] = false;
        validator["passed"] = false;

        json issues_json;
        for (const auto & issue : temp_issues[0].issues) {
          json issue_json;
          issue_json["severity"] = lanelet::validation::toString(issue.severity);
          issue_json["primitive"] = lanelet::validation::toString(issue.primitive);
          issue_json["id"] = issue.id;
          issue_json["message"] = issue.message;
          issues_json.push_back(issue_json);
        }
        validator["issues"] = issues_json;
      }

      lanelet::autoware::validation::appendIssues(issues, std::move(temp_issues));
    }

    std::cout << BOLD_ONLY << "[" << id << "] ";

    if (requirement_passed) {
      requirement["passed"] = true;
      std::cout << BOLD_GREEN << "Passed" << FONT_RESET << std::endl;
    } else {
      requirement["passed"] = false;
      std::cout << BOLD_RED << "Failed" << FONT_RESET << std::endl;
    }

    for (const auto & result : temp_validation_results) {
      if (result.second) {
        std::cout << "  - " << result.first << ": " << NORMAL_GREEN << "Passed" << FONT_RESET
                  << std::endl;
      } else {
        std::cout << "  - " << result.first << ": " << NORMAL_RED << "Failed" << FONT_RESET
                  << std::endl;
      }
    }
    lanelet::validation::printAllIssues(issues);
    std::cout << std::endl;
  }

  if (warning_count + error_count == 0) {
    std::cout << BOLD_GREEN << "No issues were found from " << FONT_RESET
              << validator_config.command_line_config.mapFile << std::endl;
  } else {
    if (warning_count > 0) {
      std::cout << BOLD_YELLOW << "Total of " << warning_count << " warnings were found from "
                << FONT_RESET << validator_config.command_line_config.mapFile << std::endl;
    }
    if (error_count > 0) {
      std::cout << BOLD_RED << "Total of " << error_count << " errors were found from "
                << FONT_RESET << validator_config.command_line_config.mapFile << std::endl;
    }
  }

  if (!validator_config.output_file_path.empty()) {
    std::string file_name = validator_config.output_file_path + "/lanelet2_validation_results.json";
    std::ofstream output_file(file_name);
    output_file << std::setw(4) << json_config;
    std::cout << "Results are output to " << file_name << std::endl;
  }

  return (warning_count + error_count == 0) ? 0 : 1;
}

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
      std::cout << "No checks found matching '"
                << meta_config.command_line_config.validationConfig.checksFilter << "'\n";
    } else {
      std::cout << "The following checks are available:\n";
      for (auto & check : checks) {
        std::cout << check << '\n';
      }
    }
    return 0;
  }

  // Check whether the map_file is specified
  if (meta_config.command_line_config.mapFile.empty()) {
    std::cout << "No map file specified" << std::endl;
    return 1;
  }

  // Validation start
  if (!meta_config.requirements_file.empty()) {
    std::ifstream input_file(meta_config.requirements_file);
    json json_config;
    input_file >> json_config;
    return new_process_requirements(json_config, meta_config);
  } else {
    auto issues = lanelet::autoware::validation::validateMap(meta_config);
    lanelet::validation::printAllIssues(issues);
    return static_cast<int>(issues.empty());
  }
}
