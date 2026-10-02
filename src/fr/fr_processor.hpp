#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/objdetect/face.hpp>
#include <string>
#include <vector>

#include "fr/face_embeddings.hpp"

struct FRDetectionMetadata;

class FRProcessor
{
    public:
        FRProcessor(const std::string& face_dataset_file_path,
                    const std::string& yunet_model_file_path,
                    const std::string& sface_model_file_path);

        void process_frame(cv::Mat& frame,
                           int scaled_width,
                           FRDetectionMetadata* metadata);

    private:
        cv::Ptr<cv::FaceDetectorYN> face_detector;
        cv::Ptr<cv::FaceRecognizerSF> face_recogniser;
        std::vector<FaceEmbeddings> face_database;
};