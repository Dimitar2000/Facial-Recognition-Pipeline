#include "fr_processor.hpp"

#include <cstdio>
#include <iostream>
#include <optional>

#include <glib.h>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#include "util/record.hpp"
#include "util/yaml.hpp"

FRProcessor::FRProcessor(const std::string& face_dataset_file_path,
                         const std::string& yunet_model_file_path,
                         const std::string& sface_model_file_path)
{
    face_database = parse_yaml_embeddings(face_dataset_file_path);
    
    if (face_database.empty())
    {
        std::cerr << "No embeddings were loaded." << std::endl;
        throw std::runtime_error("No embeddings were loaded.");
    }

    face_detector = cv::FaceDetectorYN::create(yunet_model_file_path,
                                               "",
                                               cv::Size(0, 0));
    face_recogniser = cv::FaceRecognizerSF::create(sface_model_file_path, 
                                                   "");

    std::cout << "Loaded facial recognition models." << std::endl;
}

std::vector<FRProcessor::DetectedFace> FRProcessor::process_frame(cv::Mat& frame, int scaled_width)
{
    std::vector<DetectedFace> detected_faces;
    TimeRecorder time_recorder;

    cv::Size original_size = {frame.cols, frame.rows};
    cv::Size scaled_size = {scaled_width, static_cast<int>(scaled_width / original_size.aspectRatio())};
    cv::Mat scaled_frame;
    cv::Mat faces;

    if (!scaled_size.empty() && scaled_size.area() < original_size.area())
    {
        cv::resize(frame, scaled_frame, scaled_size);
    }
    else
    {
        scaled_size = original_size;
        scaled_frame = frame;
    }

    face_detector->setInputSize(scaled_frame.size());
    
    time_recorder.start("Detection");
    face_detector->detect(scaled_frame, faces);
    time_recorder.stop();

    if (faces.empty())
    {
        std::cout << "No faces detected\n";
        return detected_faces;
    }

    for (int i = 0; i < faces.rows; i++)
    {
        cv::Mat face = faces.row(i);
        cv::Mat aligned_face;
        cv::Mat embedding;

        face_recogniser->alignCrop(scaled_frame, face, aligned_face);

        time_recorder.start("Embedding");
        face_recogniser->feature(aligned_face, embedding);
        time_recorder.stop();

        int best_matches = 0;
        int best_matches_ref_images = 0;
        double best_matches_min_similarity = MAXFLOAT;
        double best_matches_max_similarity = 0;
        double min_similarity = MAXFLOAT;
        double max_similarity = 0;
        std::string best_match_name = "Unknown";

        time_recorder.start("Matching");
        for (const auto& [name, ref_embeddings] : face_database)
        {
            int matches = 0;
            double match_min = MAXFLOAT;
            double match_max = 0;

            for (const auto& ref_embedding : ref_embeddings)
            {
                double similarity = face_recogniser->match(embedding,
                                                           ref_embedding,
                                                           cv::FaceRecognizerSF::FR_COSINE);

                if (similarity >= 0.5)
                {
                    matches++;
                    if (similarity > match_max) match_max = similarity;
                    if (similarity < match_min) match_min = similarity;
                }

                if (similarity > max_similarity) max_similarity = similarity;
                if (similarity < min_similarity) min_similarity = similarity;
            }

            if (matches > best_matches)
            {
                best_matches = matches;
                best_matches_ref_images = ref_embeddings.size();
                best_matches_min_similarity = match_min;
                best_matches_max_similarity = match_max;
                best_match_name = name;
            }
        }
        time_recorder.stop();

        float x = face.at<float>(0, 0);
        float y = face.at<float>(0, 1);
        float width = face.at<float>(0, 2);
        float height = face.at<float>(0, 3);

        double sx = static_cast<double>(original_size.width) / scaled_size.width;
        double sy = static_cast<double>(original_size.height) / scaled_size.height;


        DetectedFace face_metadata = {
            original_size,
            {
                cvRound(x * sx),
                cvRound(y * sy),
                cvRound(width * sx),
                cvRound(height * sy)
            },
            min_similarity,
            max_similarity,
            (best_matches > 0) ? std::make_optional<Identity>(Identity{
                best_match_name,
                best_matches,
                best_matches_ref_images,
                best_matches_min_similarity,
                best_matches_max_similarity
            }) : std::nullopt
        };

        detected_faces.push_back(face_metadata);
    }

    return detected_faces;
}