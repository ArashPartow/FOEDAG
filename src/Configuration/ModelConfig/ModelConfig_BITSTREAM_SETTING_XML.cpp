/*
Copyright 2023 The Foedag team

GPL License

Copyright (c) 2023 The Open-Source FPGA Foundation

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ModelConfig_BITSTREAM_SETTING_XML.h"

#include <fstream>
#include <iostream>

namespace FOEDAG {

struct PIN_TABLE_INFO {
  PIN_TABLE_INFO() {}
  PIN_TABLE_INFO(uint32_t i, bool s) : fabric_clk_index(i), is_soc(s) {}
  uint32_t fabric_clk_index = 0;
  bool is_soc = false;
  uint32_t x = 0;
  uint32_t y = 0;
  std::string type = "";
};

/*
  Generate bitstream setting XML
*/
void ModelConfig_BITSREAM_SETTINGS_XML::gen(
    const std::vector<std::string>& flag_options,
    const std::map<std::string, std::string>& options, const std::string& input,
    const std::string& output) {
  bool is_unittest = std::find(flag_options.begin(), flag_options.end(),
                               "is_unittest") != flag_options.end();
  std::vector<std::string> device_sizes =
      CFG_split_string(options.at("device_size"), "x");
  if (device_sizes.size() == 2) {
    uint32_t device_x = (uint32_t)(CFG_convert_string_to_u64(device_sizes[0]));
    uint32_t device_y = (uint32_t)(CFG_convert_string_to_u64(device_sizes[1]));
    std::ifstream design(options.at("design").c_str());
    CFG_ASSERT(design.is_open() && design.good());
    std::string line = "";
    std::map<std::string, PIN_TABLE_INFO> location_map;
    while (std::getline(design, line)) {
      CFG_get_rid_trailing_whitespace(line);
      if (line.size() > 0 && line.find("set_core_clk") == 0) {
        std::vector<std::string> words = CFG_split_string(line, " ", 0, false);
        CFG_ASSERT(words.size() == 3);
        CFG_ASSERT(words[0] == "set_core_clk");
        CFG_ASSERT(location_map.find(words[1]) == location_map.end());
        uint32_t index = (uint32_t)(CFG_convert_string_to_u64(words[2]));
        location_map[words[1]] = PIN_TABLE_INFO(index, false);
      } else if (line.size() > 0 && line.find("set_soc_clk") == 0) {
        std::vector<std::string> words = CFG_split_string(line, " ", 0, false);
        CFG_ASSERT(words.size() == 3);
        CFG_ASSERT(words[0] == "set_soc_clk");
        CFG_ASSERT(location_map.find(words[1]) == location_map.end());
        uint32_t index = (uint32_t)(CFG_convert_string_to_u64(words[2]));
        location_map[words[1]] = PIN_TABLE_INFO(index, true);
      }
    }
    design.close();
    if (location_map.size()) {
      std::ifstream pin(options.at("pin").c_str());
      CFG_ASSERT(pin.is_open() && pin.good());
      while (std::getline(pin, line)) {
        CFG_get_rid_trailing_whitespace(line);
        std::vector<std::string> words = CFG_split_string(line, ",");
        std::map<std::string, PIN_TABLE_INFO>::iterator iter;
        if (words.size() >= 14 &&
            ((words[2].size() > 0 &&
              (iter = location_map.find(words[2])) != location_map.end() &&
              !iter->second.is_soc) ||
             (words[13].size() > 0 &&
              (iter = location_map.find(words[13])) != location_map.end() &&
              iter->second.is_soc))) {
          iter->second.x = (uint32_t)(CFG_convert_string_to_u64(words[9]));
          iter->second.y = (uint32_t)(CFG_convert_string_to_u64(words[10]));
        }
      }
      pin.close();
    }
    std::ofstream oxml(output.c_str());
    if (input.size() && std::filesystem::exists(input.c_str())) {
      std::ifstream ixml(input.c_str());
      CFG_ASSERT(ixml.is_open());
      CFG_ASSERT(ixml.good());
      if (is_unittest) {
        oxml << "<!-- Original XML: Unit Test Input -->\n";
      } else {
        oxml << "<!-- Original XML: " << input.c_str() << " -->\n";
      }
      bool found_xml_end = false;
      while (std::getline(ixml, line)) {
        std::string xml_line = line;
        CFG_get_rid_whitespace(xml_line);
        if (xml_line.find("</openfpga_bitstream_setting>") == 0) {
          found_xml_end = true;
          break;
        }
        oxml << line.c_str() << "\n";
      }
      ixml.close();
      CFG_ASSERT(found_xml_end);
    } else {
      oxml << "<openfpga_bitstream_setting>\n";
    }
    oxml << "  <overwrite_bitstream>\n";
    for (auto& iter : location_map) {
      if ((iter.second.y == 1 || iter.second.y == (device_y - 2)) &&
          iter.second.x >= 2 && iter.second.x < (device_x - 2)) {
        iter.second.type = iter.second.y == 1 ? "bottom" : "top";
      } else if ((iter.second.x == 1 || iter.second.x == (device_x - 2)) &&
                 iter.second.y >= 2 && iter.second.y < (device_y - 2)) {
        iter.second.type = iter.second.x == 1 ? "left" : "right";
      }
      if (iter.second.type.size()) {
        oxml << CFG_print(
                    "    <!-- Location: %s, SOC: %d, Value: %d, X: %d, Y: %d "
                    "-->\n",
                    iter.first.c_str(), iter.second.is_soc,
                    iter.second.fabric_clk_index, iter.second.x, iter.second.y)
                    .c_str();
        for (int i = 0; i < 4; i++) {
          oxml << CFG_print(
                      "    <bit value=\"%d\" "
                      "path=\"fpga_top.grid_io_%s_%d__%d_.logical_tile_io_mode_"
                      "io__0.mem_iopad_0_clk_0[%d]\"/>\n",
                      (iter.second.fabric_clk_index & (1 << i)) ? 1 : 0,
                      iter.second.type.c_str(), iter.second.x, iter.second.y, i)
                      .c_str();
        }
      } else {
        oxml << CFG_print(
                    "    <!-- Unknown location: %s, SOC: %d, Value: %d, X: %d, "
                    "Y: %d "
                    "-->\n",
                    iter.first.c_str(), iter.second.is_soc,
                    iter.second.fabric_clk_index, iter.second.x, iter.second.y)
                    .c_str();
      }
    }
    oxml << "  </overwrite_bitstream>\n";
    oxml << "</openfpga_bitstream_setting>\n";
    oxml.close();
  }
}

}  // namespace FOEDAG
