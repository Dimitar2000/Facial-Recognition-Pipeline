#include "gpu_fr_processor.hpp"

#include <cmath>
#include <cstdio>

#include <glib.h>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#include "fr/fr_processor.hpp"

GPUFRProcessor::GPUFRProcessor(const std::string& face_dataset_file_path,
                               const std::string& yunet_model_file_path,
                               const std::string& sface_model_file_path,
                               int scaled_dim)
:   
    FRProcessor(face_dataset_file_path, 
                yunet_model_file_path, 
                sface_model_file_path, 
                scaled_dim) 
{}

std::vector<FRProcessor::DetectedFace> GPUFRProcessor::process_frame(cv::Mat& frame)
{
    return {};
}