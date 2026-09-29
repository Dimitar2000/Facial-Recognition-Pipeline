#include "json_util.hpp"

#include <fstream>
#include <iostream>

#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

std::vector<std::tuple<std::string, std::vector<std::vector<double>>>> parse_json_embeddings(std::string file_path)
{
    std::ifstream file(file_path);

    if (!file.is_open())
    {
        std::cerr << "Could not read provided file path\n";
        return {};
    }

    Json::Value data;
    std::string errs;

    // Read whole file into a buffer and parse it to JSON data
    std::stringstream buffer;
    buffer << file.rdbuf();
    buffer >> data;

    file.close();

    if (data.isNull())
    {
        std::cerr << "Could not parse to JSON object: " << errs << "\n";
        return {};
    }
    else
    {
        std::cout << "JSON file parsed successfully! Extracting embeddings ...\n";

        std::vector<std::tuple<std::string, std::vector<std::vector<double>>>> embeddings;

        for (auto name: data.getMemberNames())
        {
            std::vector<std::vector<double>> face_embeddings{};

            for (auto face_embedding_json: data[name])
            {
                std::vector<double> face_embedding{}; 

                for (auto el: face_embedding_json)
                {
                    face_embedding.push_back(el.asDouble());
                }

                face_embeddings.push_back(face_embedding);
            }

            embeddings.push_back({name, face_embeddings});

            std::cout << "Extracted " << face_embeddings.size() << " embeddings for " << name << "\n";
        }

        return embeddings;
    }
}