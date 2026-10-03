#pragma once

#include "fr/fr_processor.hpp"

#include <opencv2/core/mat.hpp>
#include <opencv2/objdetect/face.hpp>
#include <string>
#include <vector>

// Frame processor that performs Facial Recognition.
// A frame goes through the following computation steps:
//
// 1) The input frame is scaled to a smaller resolution, keeping the aspect ratio in the original.
//    The target size is either [<scaled_dim>, in_height] or [in_width, <scaled_dim>], based on 
//    which input dimension is higher, so <scaled_dim> is the higher output dimension.
//    If the input area is no greater than the target, this stage is skipped 
//
// 2) YuNet Face Detection model is run on the scaled frame to detect all faces in it.
//
// 3) Each detected face region is scaled back to original size.
//
// 4) SFace Face Recognition model is run on each detected face region to compute an embedding.
// 
// 5) Each face embedding is matched against reference SFace embeddings of one or more people and 
//    the person with the highest match score is recorded, with some statistics.
//
//
class CPUFRProcessor: public FRProcessor
{
    public:
        CPUFRProcessor(const std::string& face_dataset_file_path,
                       const std::string& yunet_model_file_path,
                       const std::string& sface_model_file_path,
                       int scaled_dim);

        virtual std::vector<FRProcessor::DetectedFace> process_frame(cv::Mat& frame) override;
};