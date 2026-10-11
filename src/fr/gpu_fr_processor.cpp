#include "gpu_fr_processor.hpp"

#include <cmath>
#include <cstdio>

#include <glib.h>
#include <iostream>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/dnn/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

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
{
    yunet = cv::dnn::readNet(yunet_model_file_path);

    yunet.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    yunet.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
}

std::vector<FRProcessor::DetectedFace> GPUFRProcessor::process_frame(cv::Mat& frame)
{
    cv::Size original_size = {frame.cols, frame.rows};
    cv::Size scaled_size = {scaled_dim, static_cast<int>(scaled_dim / original_size.aspectRatio())};

    /*
     * Resize frame if too large
     */
    cv::Mat scaled_frame;
    float dim_scale_factor;

    if (!scaled_size.empty() && scaled_size.area() < original_size.area())
    {
        cv::resize(frame, scaled_frame, scaled_size);
        dim_scale_factor = original_size.width * 1.0f / scaled_size.width;
    }
    else
    {
        scaled_size = original_size;
        scaled_frame = frame;
        dim_scale_factor = 1;
    }

    /*
     * Detect faces, decode, and filter out low scores
     */
    auto detected_faces = detect_faces(scaled_frame, dim_scale_factor);

    /*
     * Create face embeddings, match with references, and identify faces
     */
    auto recognised_faces = recognise_faces(frame, detected_faces);

    return recognised_faces;
}

std::vector<cv::Mat> GPUFRProcessor::detect_faces(cv::Mat& scaled_frame, float dim_scale_factor)
{
    const cv::Size input_size(320, 320);
    
    const float scale_x = static_cast<float>(scaled_frame.cols) / input_size.width;
    const float scale_y = static_cast<float>(scaled_frame.rows) / input_size.height;

    cv::Mat blob = cv::dnn::blobFromImage(
        scaled_frame,
        1.0,
        input_size,
        cv::Scalar(),
        false,
        false,
        CV_32F
    );

    std::vector<cv::Mat> outputs;
    std::vector<cv::Mat> faces;

    /*
     * Run YuNet on the frame, using GPU and moving results to host memory
     */
    yunet.setInput(blob);
    yunet.forward(outputs, yunet.getUnconnectedOutLayersNames());

    /*
     * Decode YuNet output and filter out faces with low score
     */
    const int strides[] = {8, 16, 32};

    for (int s = 0; s < 3; ++s)
    {
        const cv::Mat& cls_s = outputs[s];
        const cv::Mat& obj_s = outputs[3 + s];
        const cv::Mat& bbox_s = outputs[6 + s];
        const cv::Mat& kps_s = outputs[9 + s];

        int stride = strides[s];

        int height = input_size.height / stride;
        int width  = input_size.width / stride;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int idx = y * width + x;

                if (idx >= cls_s.size[1]) continue;

                float cls_score = cls_s.at<float>(0, idx, 0);
                float obj_score = obj_s.at<float>(0, idx, 0);
                float score = cls_score;

                if (score < 0.8f)
                    continue;

                /*
                 * Decode YuNet face data
                 */
                cv::Mat face(1, 15, CV_32F);
                float* face_data = face.ptr<float>();

                float cx = x * stride;
                float cy = y * stride;

                float dx = bbox_s.at<float>(0, idx, 0) * stride;
                float dy = bbox_s.at<float>(0, idx, 1) * stride;
                float dw = bbox_s.at<float>(0, idx, 2);
                float dh = bbox_s.at<float>(0, idx, 3);

                float w = std::exp(dw) * stride;
                float h = std::exp(dh) * stride;
                
                face_data[0] = dim_scale_factor * scale_x * (cx + dx - w * 0.5f);
                face_data[1] = dim_scale_factor * scale_y * (cy + dy - h * 0.5f);
                face_data[2] = dim_scale_factor * scale_x * w;
                face_data[3] = dim_scale_factor * scale_y * h;

                for (int k = 0; k < 5; ++k)
                {
                    float lx = kps_s.at<float>(0, idx, k * 2 + 0);
                    float ly = kps_s.at<float>(0, idx, k * 2 + 1);

                    face_data[4 + 2*k]     = dim_scale_factor * scale_x * (lx * stride + cx);
                    face_data[4 + 2*k + 1] = dim_scale_factor * scale_y * (ly * stride + cy);
                }

                face_data[14] = score;

                faces.push_back(face);
            }
        }
    }

    return faces;
}

std::vector<FRProcessor::DetectedFace> GPUFRProcessor::recognise_faces(cv::Mat& frame, 
                                                                       const std::vector<cv::Mat>& detected_faces)
{
    std::vector<FRProcessor::DetectedFace> recognised_faces;
    recognised_faces.reserve(detected_faces.size());

    for (int i = 0; i < detected_faces.size(); i++)
    {
        cv::Mat face = detected_faces[i];
        cv::Mat aligned_face;
        cv::Mat embedding;

        /*
         * Align to frame
         */
        face_recogniser->alignCrop(frame, face, aligned_face);

        face_recogniser->feature(aligned_face, embedding);

        int best_matches = 0;
        int best_matches_ref_images = 0;
        double best_matches_min_similarity = MAXFLOAT;
        double best_matches_max_similarity = 0;
        double min_similarity = MAXFLOAT;
        double max_similarity = 0;
        std::string best_match_name = "Unknown";

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

        DetectedFace face_metadata = {
            {
                cvRound(face.at<float>(0)),
                cvRound(face.at<float>(1)),
                cvRound(face.at<float>(2)),
                cvRound(face.at<float>(3))
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

        recognised_faces.push_back(face_metadata);
    }

    return recognised_faces;
}
