#pragma once

#include <string>
#include <vector>

#include "fr/cpu_fr_processor.hpp"

std::vector<CPUFRProcessor::FaceEmbeddings> parse_yaml_embeddings(std::string file_path);