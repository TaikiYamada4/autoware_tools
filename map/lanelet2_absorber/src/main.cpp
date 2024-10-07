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

#include <autoware_lanelet2_extension/projection/mgrs_projector.hpp>

#include <boost/optional.hpp>

#include <lanelet2_core/primitives/Lanelet.h>
#include <lanelet2_core/utility/Utilities.h>
#include <lanelet2_io/Io.h>
#include <yaml-cpp/yaml.h>

namespace lanelet
{
// Function to extract the origin from the YAML file
Origin extractOriginFromYaml(const std::string & yaml_file_path)
{
  // Load the YAML file
  YAML::Node config = YAML::LoadFile(yaml_file_path);

  // Navigate through the layers to access 'map_origin'
  YAML::Node map_origin = config["/**"]["ros__parameters"]["map_origin"];

  // Extract latitude and longitude from the 'map_origin' section
  double latitude = map_origin["latitude"].as<double>();
  double longitude = map_origin["longitude"].as<double>();

  // Return a Lanelet2 origin using the latitude and longitude
  return Origin({latitude, longitude});
}

void get_one_step_deeper(LaneletMapConstPtr base_map, LaneletMapPtr output_map, Ids linestring_ids)
{  //, Ids lanelet_ids, Ids polygon_ids, Ids regulatory_element_ids) {
  for (const auto & reg_elem : base_map->regulatoryElementLayer) {
    for (const Id id : linestring_ids) {
      Optional<ConstLineString3d> ls = reg_elem->find<ConstLineString3d>(id);
      if (ls) {
        output_map->add(std::const_pointer_cast<RegulatoryElement>(reg_elem));
      }
    }
  }
}
}  // namespace lanelet

int main(int argc, char * argv[])
{
  // Check if the OSM file path argument is provided
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <path_to_map.osm> <path_to_map_config.yaml>"
              << std::endl;
    return 1;
  }

  // Load the lanelet2 map from an OSM file
  std::string osm_file_path = argv[1];
  std::string yaml_file_path = argv[2];

  lanelet::Origin map_origin;
  try {
    map_origin = lanelet::extractOriginFromYaml(yaml_file_path);
  } catch (const YAML::Exception & e) {
    std::cerr << e.what() << std::endl;
  }

  lanelet::projection::MGRSProjector projector(map_origin);
  lanelet::LaneletMapPtr laneletMap = lanelet::load(osm_file_path, projector);

  // Create a new LaneletMap to store the exported lanelet
  lanelet::LaneletMapPtr exportMap = std::make_shared<lanelet::LaneletMap>();

  // Specify the lanelet ID you want to export
  lanelet::Id targetLaneletId = 10;  // Replace with your specific lanelet ID
  lanelet::Ids target_traffic_light_ids = {1024};

  // Retrieve the specific lanelet
  lanelet::Lanelet targetLanelet;
  try {
    // Add the target lanelet to the new map
    targetLanelet = laneletMap->laneletLayer.get(targetLaneletId);
    exportMap->add(targetLanelet);
    // Get the traffic_light linestring regulatory element
    lanelet::get_one_step_deeper(laneletMap, exportMap, target_traffic_light_ids);
  } catch (const lanelet::LaneletError & err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }

  // Export the new map to a new OSM file
  std::string output_file_name = "exported_lanelet.osm";
  lanelet::write(output_file_name, *exportMap, projector);

  std::cout << "Lanelet " << targetLaneletId << " exported successfully to 'exported_lanelet.osm'!"
            << std::endl;

  return 0;
}
