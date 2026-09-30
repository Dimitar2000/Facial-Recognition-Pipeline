#include "yaml_util.hpp"

#include <glib.h>
#include <iostream>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/persistence.hpp>

std::vector<FaceEmbeddings> parse_yaml_embeddings(std::string file_path)
{
    cv::FileStorage fs(
        file_path,
        cv::FileStorage::READ
    );

    if (!fs.isOpened())
    {
        std::cerr << "Could not open " << file_path  << std::endl;
        return {};
    }

    std::vector<FaceEmbeddings> face_database;

    cv::FileNode faces = fs["faces"];

    for (const auto& face_data: faces)
    {
        FaceEmbeddings face_embeddings;

        face_data["name"]       >> face_embeddings.name;
        face_data["embeddings"] >> face_embeddings.embeddings;
        face_database.push_back(std::move(face_embeddings));

        std::cout << "Loaded " << face_database.back().embeddings.size() << " face embeddings for " << face_database.back().name << ".\n";
    }

    fs.release();

    return face_database;
}