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

#include "lanelet2_map_validator/utils.hpp"

#include <string>

std::string snake_to_upper_camel(const std::string & snake_case)
{
  std::string camel_case;
  bool capitalize_next = true;

  for (char ch : snake_case) {
    if (ch == '_') {
      capitalize_next = true;
    } else {
      camel_case += capitalize_next ? std::toupper(ch) : ch;
      capitalize_next = false;
    }
  }
  return camel_case;
}
