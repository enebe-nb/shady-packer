#pragma once
#include "../Core/resource.hpp"
#include <filesystem>
#include <cxxopts.hpp>

namespace ShadyCli {
	class Command {
    private:
        cxxopts::Options options;
        static void printHelp(const char*, ShadyCli::Command*);
	public:
		static Command* parseArguments(int, char*[]);
        virtual void buildOptions(cxxopts::Options&) = 0;
        virtual void run(const cxxopts::Options&) = 0;
		inline void run() { run(options); }
        inline Command(std::string name, std::string help) : options(name, help) {}
	};

	class ConvertCommand : public Command {
	private:
        void processFile(std::filesystem::path, std::filesystem::path, bool);
	public:
		inline ConvertCommand() : Command("convert", "Converts game files into their encrypted/decrypted counterpart.") {}
        void buildOptions(cxxopts::Options&) override;
		void run(const cxxopts::Options&) override;
	};

	class MergeCommand : public Command {
	private:
        void processPalette(ShadyCore::Palette*, std::filesystem::path);
        void processPattern(ShadyCore::Pattern*, std::filesystem::path);
	public:
		inline MergeCommand() : Command("merge", "Merge many kind of files") {}
        void buildOptions(cxxopts::Options&) override;
		void run(const cxxopts::Options&) override;
	};

	class PackCommand : public Command {
	public:
		inline PackCommand() : Command("pack", "Compress/Extract/Convert/Concat package files.") {}
        void buildOptions(cxxopts::Options&) override;
		void run(const cxxopts::Options&) override;
	};

	class ScaleCommand : public Command {
	private:
		std::string prefix;
		void (*scalingAlgorithm)(ShadyCore::Image*, uint8_t* source);
		void process(std::filesystem::path, std::filesystem::path);
		void process(ShadyCore::Pattern* pattern);
	public:
		inline ScaleCommand() : Command("scale", "Scale sprites and increase textures sizes in pattern files.") {}
        void buildOptions(cxxopts::Options&) override;
		void run(const cxxopts::Options&) override;
	};
}
