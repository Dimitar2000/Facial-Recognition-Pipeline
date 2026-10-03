#include "fr/fr_processor.hpp"

#include <cmath>
#include <cstdio>
#include <iostream>

#include <glib.h>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#include "util/yaml.hpp"

FRProcessor::FRProcessor(const std::string& face_dataset_file_path,
                         const std::string& yunet_model_file_path,
                         const std::string& sface_model_file_path,
                         int scaled_dim)
:   
    scaled_dim(scaled_dim)
{
    face_database = parse_yaml_embeddings(face_dataset_file_path);
    
    if (face_database.empty())
    {
        std::cerr << "No embeddings were loaded." << std::endl;
        throw std::runtime_error("No embeddings were loaded.");
    }

    std::cout << "[FR Processor] Loaded embeddings." << std::endl;

    face_detector = cv::FaceDetectorYN::create(yunet_model_file_path,
                                               "",
                                               cv::Size(0, 0));
    face_recogniser = cv::FaceRecognizerSF::create(sface_model_file_path, 
                                                   "");

    std::cout << "[FR Processor] Loaded facial recognition models." << std::endl;
}