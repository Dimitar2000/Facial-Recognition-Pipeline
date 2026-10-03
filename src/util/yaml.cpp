#include "yaml.hpp"

#include <iostream>
#include <opencv2/core/persistence.hpp>
#include <utility>

std::vector<CPUFRProcessor::FaceEmbeddings> parse_yaml_embeddings(std::string file_path)
{
    cv::FileStorage fs(
        file_path,
        cv::FileStorage::READ
    );

    if (!fs.isOpened())
    {
        std::cerr << "Could not open " << file_path  << std::endl;
        
        throw std::runtime_error("Could not open " + file_path);
    }

    std::vector<CPUFRProcessor::FaceEmbeddings> face_database;

    cv::FileNode faces = fs["faces"];

    for (const auto& face_data: faces)
    {
        CPUFRProcessor::FaceEmbeddings face_embeddings;

        face_data["name"]       >> face_embeddings.name;
        face_data["embeddings"] >> face_embeddings.embeddings;
        face_database.push_back(std::move(face_embeddings));

        std::cout << "[YAML] Read " << face_database.back().embeddings.size() << " face embeddings for " << face_database.back().name << ".\n";
    }

    fs.release();

    return face_database;
}