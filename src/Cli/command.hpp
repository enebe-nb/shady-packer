#pragma once
#include "../Core/resource.hpp"
#include <filesystem>
#include <cxxopts.hpp>

namespace ShadyCli {
    class Command {
    protected:
        cxxopts::Options options;
        static void printHelp(const char*, ShadyCli::Command*);
    public:
        static bool run(int, char*[]);
        virtual void buildOptions() = 0;
        virtual bool run(const cxxopts::ParseResult&) = 0;
        inline Command(std::string name, std::string help) : options(name, help) {}
    };

    class ConvertCommand : public Command {
    private:
        void processFile(std::filesystem::path, std::filesystem::path, bool);
    public:
        inline ConvertCommand() : Command("convert", "Converts game files into their encrypted/decrypted counterpart.") {}
        void buildOptions() override;
        bool run(const cxxopts::ParseResult&) override;
    };

    class MergeCommand : public Command {
    private:
        void processPalette(ShadyCore::Palette*, std::filesystem::path);
        void processPattern(ShadyCore::Schema*, std::filesystem::path);
    public:
        inline MergeCommand() : Command("merge", "Merge many kind of files") {}
        void buildOptions() override;
        bool run(const cxxopts::ParseResult&) override;
    };

    class PackCommand : public Command {
    public:
        inline PackCommand() : Command("pack", "Compress/Extract/Convert/Concat package files.") {}
        void buildOptions() override;
        bool run(const cxxopts::ParseResult&) override;
    };

    class ScaleCommand : public Command {
    private:
        std::string prefix;
        void (*scalingAlgorithm)(ShadyCore::Image*, uint8_t* source);
        void process(std::filesystem::path, std::filesystem::path);
        void process(ShadyCore::Schema* pattern);
    public:
        inline ScaleCommand() : Command("scale", "Scale sprites and increase textures sizes in pattern files.") {}
        void buildOptions() override;
        bool run(const cxxopts::ParseResult&) override;
    };
}
