#pragma once
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace ShadyCli {
	class Command {
	public:
		static Command* parseArguments(int, char*[]);
		virtual void run() = 0;
	};

	class ConvertCommand : public Command {
	private:
		std::vector<std::string> files;
		std::string output;
		bool copyUnknown;
		void processFile(boost::filesystem::path, boost::filesystem::path&);
	public:
		ConvertCommand(int, char*[]);
		void run();
	};

	class ExtractCommand : public Command {
	private:
		std::string source;
		std::string target;
        void processFile(boost::filesystem::path);
	public:
		ExtractCommand(int, char*[]);
		void run();
	};

	class PackCommand : public Command {
	private:
		ShadyCore::Package package;
		ShadyCore::Package::Mode mode;
		std::string output;
		std::vector<std::string> files;
		void processFile(boost::filesystem::path);
	public:
		PackCommand(int, char*[]);
		void run();
	};
}
