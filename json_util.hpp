#pragma once

#include <string>
#include <tuple>
#include <vector>

std::vector<std::tuple<std::string, std::vector<std::vector<double>>>> parse_json_embeddings(std::string file_path);