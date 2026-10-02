#pragma once

#include <opencv2/core/mat.hpp>
#include <string>
#include <vector>

#include "fr/face_embeddings.hpp"

std::vector<FaceEmbeddings> parse_yaml_embeddings(std::string file_path);