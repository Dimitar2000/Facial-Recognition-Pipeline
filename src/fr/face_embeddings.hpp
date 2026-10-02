#pragma once

#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

struct FaceEmbeddings
{
    std::string name;
    std::vector<cv::Mat> embeddings; 
};
