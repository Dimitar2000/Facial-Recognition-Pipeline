#pragma once

#include <string>
#include <vector>

#include "fr/fr_processor.hpp"

std::vector<FRProcessor::FaceEmbeddings> parse_yaml_embeddings(std::string file_path);