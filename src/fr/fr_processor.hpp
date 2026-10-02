#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/objdetect/face.hpp>
#include <optional>
#include <string>
#include <vector>

class FRProcessor
{
    public:
        struct FaceEmbeddings
        {
            std::string name;
            std::vector<cv::Mat> embeddings; 
        };

        struct Identity {
            std::string name;
            int reference_matches;
            int reference_images;
            double min_similarity;
            double max_similarity;
        };

        struct DetectedFace {
            cv::Size frame_size;
            cv::Rect face_rect;
            
            double min_similarity;
            double max_similarity;
            std::optional<Identity> identity;
        };

        FRProcessor(const std::string& face_dataset_file_path,
                    const std::string& yunet_model_file_path,
                    const std::string& sface_model_file_path);

        std::vector<DetectedFace> process_frame(cv::Mat& frame, 
                                                int scaled_width);

    private:
        cv::Ptr<cv::FaceDetectorYN> face_detector;
        cv::Ptr<cv::FaceRecognizerSF> face_recogniser;
        std::vector<FaceEmbeddings> face_database;
};