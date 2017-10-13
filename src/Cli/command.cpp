//#include "../Core/package.hpp"
//#include "../Core/resource/readerwriter.hpp"
#include "command.hpp"
//#include <boost/filesystem.hpp>
//#include <iostream>
//#include <png.h>

static inline ShadyCli::Command* getCommand(const char* arg) {
    if (strcmp(arg, "convert") == 0) {
		return new ShadyCli::ConvertCommand;
	} else if (strcmp(arg, "pack") == 0) {
		return new ShadyCli::PackCommand;
	} else if (strcmp(arg, "merge") == 0) {
		return new ShadyCli::MergeCommand;
	} return 0;
}

void ShadyCli::Command::printHelp(const char* bin, Command* command) {
    if (command) {
        command->buildOptions(command->options);
        printf("%s\n", command->options.help({""}).c_str());
    } else {
        printf("Usage:\n"
            "  %s <command> [COMMAND_OPTIONS]\n\n"
            "List of commands:\n"
            "  help     Prints help for commands.\n"
            "  convert  Converts game files into their encrypted/decrypted\n"
            "           counterpart.\n"
            "  merge    Merge many kind of files.\n"
            "  pack     Pack game files into package files.\n"
            "\n", bin);
    }
}

ShadyCli::Command* ShadyCli::Command::parseArguments(int argc, char* argv[]) {
    ShadyCli::Command* command = 0;
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        printHelp(argv[0], argc >= 3 ? getCommand(argv[2]) : 0);
        return 0;
    } else {
        command = getCommand(argv[1]);
    }

    if (command) {
        command->buildOptions(command->options);
        command->options.parse(--argc, argv = &argv[1]);
    } else {
        printf("Unknown command: %s. Use `%s help` for help.\n", argv[1], argv[0]);
    } return command;
}


int main(int argc, char* argv[]) {
	ShadyCli::Command* command = ShadyCli::Command::parseArguments(argc, argv);
	if (command) {
        command->run();
        delete command;
    }
    return 0;
}
