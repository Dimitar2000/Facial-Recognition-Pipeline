#pragma once

#include <opencv2/core/mat.hpp>
#include <string>
#include <vector>

struct FaceEmbeddings
{
    std::string name;
    std::vector<cv::Mat> embeddings; 
};

std::vector<FaceEmbeddings> parse_yaml_embeddings(std::string file_path);