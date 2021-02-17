#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"
#include <math.h>
#include <fstream>

namespace {
    typedef void (*funcPtr) (ShadyCore::Image*, uint8_t* source);
    
    static void nearestAlgorithm (ShadyCore::Image* image, uint8_t* source) {
        size_t sourceSize = image->getWidth() * image->getHeight();
        image->initialize(image->getWidth() * 2, image->getHeight() * 2, ceil(image->getWidth() / 2.f) * 4, image->getBitsPerPixel());
        
        for (int j = 0; j < image->getHeight(); ++j) {
            for (int i = 0; i < image->getWidth(); ++i) {
                image->getRawImage()[j * image->getWidth() + i] = source[j / 2 * image->getWidth() / 2 + i / 2];
            }
        }
    }

    static void epxAlgorithm (ShadyCore::Image* image, uint8_t* source) {
        uint32_t width = image->getWidth();
        uint32_t height = image->getHeight();
        image->initialize(width * 2, height * 2, ceil(width / 2.f) * 4, image->getBitsPerPixel());
        
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                int index = j * width + i;
                image->getRawImage()[(j * 2    ) * width * 2 + (i * 2    )] = 
                    i > 0 && j > 0 && source[(j - 1) * width + i] == source[(j) * width + i - 1]
                    ? source[(j - 1) * width + i] : source[index];
                image->getRawImage()[(j * 2    ) * width * 2 + (i * 2 + 1)] = 
                    i < width - 1 && j > 0 && source[(j - 1) * width + i] == source[(j) * width + i + 1]
                    ? source[(j - 1) * width + i] : source[index];
                image->getRawImage()[(j * 2 + 1) * width * 2 + (i * 2    )] = 
                    i > 0 && j < height - 1 && source[(j + 1) * width + i] == source[(j) * width + i - 1]
                    ? source[(j + 1) * width + i] : source[index];
                image->getRawImage()[(j * 2 + 1) * width * 2 + (i * 2 + 1)] = 
                    i < width - 1 && j < height - 1 && source[(j + 1) * width + i] == source[(j) * width + i + 1]
                    ? source[(j + 1) * width + i] : source[index];
            }
        }
    }

    static inline uint8_t eagleTest(uint8_t s, uint8_t v, uint8_t h, uint8_t d) {
        return v == h && v == d ? v : s;
    }

    static void eagleAlgorithm (ShadyCore::Image* image, uint8_t* source) {
        uint32_t width = image->getWidth();
        uint32_t height = image->getHeight();
        image->initialize(width * 2, height * 2, ceil(width / 2.f) * 4, image->getBitsPerPixel());
        
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                int index = j * width + i;
                image->getRawImage()[(j * 2    ) * width * 2 + (i * 2    )] = 
                    i > 0 && j > 0
                    ? eagleTest(source[index], source[index - width], source[index - 1], source[index - width - 1]) : source[index];
                image->getRawImage()[(j * 2    ) * width * 2 + (i * 2 + 1)] = 
                    i < width - 1 && j > 0
                    ? eagleTest(source[index], source[index - width], source[index + 1], source[index - width + 1]) : source[index];
                image->getRawImage()[(j * 2 + 1) * width * 2 + (i * 2    )] = 
                    i > 0 && j < height - 1
                    ? eagleTest(source[index], source[index + width], source[index - 1], source[index + width - 1]) : source[index];
                image->getRawImage()[(j * 2 + 1) * width * 2 + (i * 2 + 1)] = 
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
    
    std::ifstream input(filename.string(), std::fstream::binary);
    const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.generic_string().c_str(), input);
    if (type == ShadyCore::FileType::TYPE_IMAGE) {
        if (std::strncmp((char*)filename.filename().c_str(), prefix.c_str(), prefix.length()) == 0) return;
        ShadyCore::Image* image = (ShadyCore::Image*)ShadyCore::readResource(type, input);
        if (image->getBitsPerPixel() == 8) {
            uint8_t* imageData = new uint8_t[image->getWidth() * image->getHeight()];
            std::memcpy(imageData, image->getRawImage(), image->getWidth() * image->getHeight());
            scalingAlgorithm(image, imageData);
            delete[] imageData;

	        std::filesystem::path targetName = targetPath / prefix; targetName += filename.filename();
            std::filesystem::create_directories(targetPath);
	        std::ofstream output(targetName.string(), std::fstream::binary);
            ShadyCore::writeResource(image, output, type.isEncrypted);
        } delete image;
    } else if (type == ShadyCore::FileType::TYPE_PATTERN) {
        ShadyCore::Pattern* pattern = (ShadyCore::Pattern*)ShadyCore::readResource(type, input);
        process(pattern);

        std::filesystem::path targetName = targetPath / filename.filename();
        std::filesystem::create_directories(targetPath);
        std::ofstream output(targetName.string(), std::fstream::binary);
        ShadyCore::writeResource(pattern, output, type.isEncrypted);
        delete pattern;
    }
}

void ShadyCli::ScaleCommand::process(ShadyCore::Pattern* pattern) {
    for (int i = 0; i < pattern->getSequenceCount(); ++i) {
        if (pattern->getSequence(i).getType() == ShadyCore::Sequence::SEQUENCE_MOVE) {
            ShadyCore::Sequence::Move& move = pattern->getSequence(i).asMove();
            for (int j = 0; j < move.getFrameCount(); ++j) {
                ShadyCore::Frame::Move& frame = move.getFrame(j).asMove();
                if (frame.getRenderGroup() != 0) continue;
                frame.setRenderGroup(1);
                std::string imageName(prefix + frame.getImageName());
                frame.setImageName(imageName.c_str());
                frame.setTexWidth(frame.getTexWidth() * 2);
                frame.setTexHeight(frame.getTexHeight() * 2);
                frame.setTexOffsetX(frame.getTexOffsetX() * 2);
                frame.setTexOffsetY(frame.getTexOffsetY() * 2);
            }
        }
    }
}

void ShadyCli::ScaleCommand::buildOptions(cxxopts::Options& options) {
	options.add_options()
		("o,output", "Target directory to save", cxxopts::value<std::string>()->default_value(std::filesystem::current_path().string()))
		("p,prefix", "Prefix to append to image filenames", cxxopts::value<std::string>()->default_value("scaled-"))
		("a,algorithm", "Scaling algorithm to apply to the images. Available options: nearest, epx or eagle.", cxxopts::value<std::string>()->default_value("nearest"))
		("files", "Source directories or files", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

void ShadyCli::ScaleCommand::run(const cxxopts::Options& options) {
    auto& files = options["files"].as<std::vector<std::string> >();
	std::filesystem::path outputPath(options["output"].as<std::string>());
    prefix = options["prefix"].as<std::string>();
    
    scalingAlgorithm = getScalingAlgorithm(options["algorithm"].as<std::string>());
    if (!scalingAlgorithm) {
        printf("Algorithm %s not found.\n", options["algorithm"].as<std::string>().c_str());
        return;
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
}