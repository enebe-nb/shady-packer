#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"
#include <math.h>
#include <fstream>

namespace {
    typedef void (*funcPtr) (ShadyCore::Image*, uint8_t* source);

    static void nearestAlgorithm (ShadyCore::Image* image, uint8_t* source) {
        size_t sourceSize = image->width * image->height;
        image->initialize(image->width * 2, image->height * 2, ceil(image->width / 2.f) * 4, image->bitsPerPixel);
        
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                image->raw[j * image->width + i] = source[j / 2 * image->width / 2 + i / 2];
            }
        }
    }

    static void epxAlgorithm (ShadyCore::Image* image, uint8_t* source) {
        uint32_t width = image->width;
        uint32_t height = image->height;
        image->initialize(width * 2, height * 2, ceil(width / 2.f) * 4, image->bitsPerPixel);
        
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                int index = j * width + i;
                image->raw[(j * 2    ) * width * 2 + (i * 2    )] = 
                    i > 0 && j > 0 && source[(j - 1) * width + i] == source[(j) * width + i - 1]
                    ? source[(j - 1) * width + i] : source[index];
                image->raw[(j * 2    ) * width * 2 + (i * 2 + 1)] = 
                    i < width - 1 && j > 0 && source[(j - 1) * width + i] == source[(j) * width + i + 1]
                    ? source[(j - 1) * width + i] : source[index];
                image->raw[(j * 2 + 1) * width * 2 + (i * 2    )] = 
                    i > 0 && j < height - 1 && source[(j + 1) * width + i] == source[(j) * width + i - 1]
                    ? source[(j + 1) * width + i] : source[index];
                image->raw[(j * 2 + 1) * width * 2 + (i * 2 + 1)] = 
                    i < width - 1 && j < height - 1 && source[(j + 1) * width + i] == source[(j) * width + i + 1]
                    ? source[(j + 1) * width + i] : source[index];
            }
        }
    }

    static inline uint8_t eagleTest(uint8_t s, uint8_t v, uint8_t h, uint8_t d) {
        return v == h && v == d ? v : s;
    }

    static void eagleAlgorithm (ShadyCore::Image* image, uint8_t* source) {
        uint32_t width = image->width;
        uint32_t height = image->height;
        image->initialize(width * 2, height * 2, ceil(width / 2.f) * 4, image->bitsPerPixel);
        
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                int index = j * width + i;
                image->raw[(j * 2    ) * width * 2 + (i * 2    )] = 
                    i > 0 && j > 0
                    ? eagleTest(source[index], source[index - width], source[index - 1], source[index - width - 1]) : source[index];
                image->raw[(j * 2    ) * width * 2 + (i * 2 + 1)] = 
                    i < width - 1 && j > 0
                    ? eagleTest(source[index], source[index - width], source[index + 1], source[index - width + 1]) : source[index];
                image->raw[(j * 2 + 1) * width * 2 + (i * 2    )] = 
                    i > 0 && j < height - 1
                    ? eagleTest(source[index], source[index + width], source[index - 1], source[index + width - 1]) : source[index];
                image->raw[(j * 2 + 1) * width * 2 + (i * 2 + 1)] = 
                    i < width - 1 && j < height - 1
                    ? eagleTest(source[index], source[index + width], source[index + 1], source[index + width + 1]) : source[index];
            }
        }
    }

    static funcPtr getScalingAlgorithm(const std::string& name) {
        if (name == "nearest") return nearestAlgorithm;
        else if (name == "epx") return epxAlgorithm;
        else if (name == "eagle") return eagleAlgorithm;
        else return 0;
    }
}

void ShadyCli::ScaleCommand::process(std::filesystem::path filename, std::filesystem::path targetPath) {
    if (!std::filesystem::is_regular_file(filename)) return; // ignore
    
    std::ifstream input(filename, std::fstream::binary);
    ShadyCore::FileType type = ShadyCore::FileType::get(filename.string().c_str());
    if (type == ShadyCore::FileType::TYPE_IMAGE) {
        ShadyCore::Image* image = (ShadyCore::Image*)ShadyCore::createResource(ShadyCore::FileType::TYPE_IMAGE);
        ShadyCore::getResourceReader(type)(image, input);
        if (image->bitsPerPixel == 8) {
            uint8_t* imageData = new uint8_t[image->width * image->height];
            std::memcpy(imageData, image->raw, image->width * image->height);
            scalingAlgorithm(image, imageData);
            delete[] imageData;

            std::filesystem::path targetName = targetPath / prefix; targetName += filename.filename();
            std::filesystem::create_directories(targetPath);
            std::ofstream output(targetName.string(), std::fstream::binary);
            ShadyCore::getResourceWriter(type)(image, output);
        } ShadyCore::destroyResource(ShadyCore::FileType::TYPE_IMAGE, image);
    } else if (type == ShadyCore::FileType::TYPE_SCHEMA) {
        ShadyCore::FileType::Format format = filename.extension() == ".dat" ? ShadyCore::FileType::SCHEMA_GAME_PATTERN : ShadyCore::FileType::SCHEMA_XML;
        ShadyCore::Schema* schema = (ShadyCore::Schema*)ShadyCore::createResource(ShadyCore::FileType::TYPE_SCHEMA);
        ShadyCore::getResourceReader(type)(schema, input);
        process(schema);

        std::filesystem::path targetName = targetPath / filename.filename();
        std::filesystem::create_directories(targetPath);
        std::ofstream output(targetName.string(), std::fstream::binary);
        ShadyCore::getResourceWriter(type)(schema, output);
        ShadyCore::destroyResource(ShadyCore::FileType::TYPE_SCHEMA, schema);
    }
}

void ShadyCli::ScaleCommand::process(ShadyCore::Schema* pattern) {
    for (int i = 0; i < pattern->objects.size(); ++i) {
        if (pattern->objects[i]->getType() == 9) {
            auto move = ((ShadyCore::Schema::Sequence*)pattern->objects[i]);
            for (int j = 0; j < move->frames.size(); ++j) {
                auto frame = (ShadyCore::Schema::Sequence::MoveFrame*)move->frames[j];
                if (frame->renderGroup != 0) continue;
                frame->renderGroup = 1;
                // TODO
                // std::string imageName(prefix + frame.getImageName());
                // frame.setImageName(imageName.c_str());
                // frame.setTexWidth(frame.getTexWidth() * 2);
                // frame.setTexHeight(frame.getTexHeight() * 2);
                // frame.setTexOffsetX(frame.getTexOffsetX() * 2);
                // frame.setTexOffsetY(frame.getTexOffsetY() * 2);
            }
        }
    }
}

void ShadyCli::ScaleCommand::buildOptions() {
    options.add_options()
        ("o,output", "Target directory to save", cxxopts::value<std::string>()->default_value(std::filesystem::current_path().string()))
        ("p,prefix", "Prefix to append to image filenames", cxxopts::value<std::string>()->default_value("scaled-"))
        ("a,algorithm", "Scaling algorithm to apply to the images. Available options: nearest, epx or eagle.", cxxopts::value<std::string>()->default_value("nearest"))
        ("files", "Source directories or files", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

bool ShadyCli::ScaleCommand::run(const cxxopts::ParseResult& result) {
    auto& files = result["files"].as<std::vector<std::string> >();
    std::filesystem::path outputPath(result["output"].as<std::string>());
    prefix = result["prefix"].as<std::string>();
    
    scalingAlgorithm = getScalingAlgorithm(result["algorithm"].as<std::string>());
    if (!scalingAlgorithm) {
        printf("Algorithm %s not found.\n", result["algorithm"].as<std::string>().c_str());
        return false;
    }

    size_t size = files.size();
    for (size_t i = 0; i < size; ++i) {
        std::filesystem::path path(files[i]);
        if (std::filesystem::is_directory(path)) {
            for (std::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!std::filesystem::is_directory(iter->path())) {
                    process(iter->path(), outputPath / std::filesystem::relative(iter->path().parent_path(), path));
                }
            }
        } else if (std::filesystem::exists(path)) {
            process(path, outputPath);
        }
    }

    return true;
}