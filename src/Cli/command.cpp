#include "command.hpp"

static inline ShadyCli::Command* getCommand(const char* arg) {
    if (strcmp(arg, "convert") == 0) {
        return new ShadyCli::ConvertCommand;
    } else if (strcmp(arg, "pack") == 0) {
        return new ShadyCli::PackCommand;
    } else if (strcmp(arg, "merge") == 0) {
        return new ShadyCli::MergeCommand;
    } else if (strcmp(arg, "scale") == 0) {
        return new ShadyCli::ScaleCommand;
    } else if (strcmp(arg, "list") == 0) {
        return new ShadyCli::ListCommand;
    }
    return nullptr;
}

void ShadyCli::Command::printHelp(const char* bin, Command* command) {
    if (command) {
        command->buildOptions();
        printf("%s\n", command->options.help({""}).c_str());
    } else {
        printf("Usage:\n"
            "  %s <command> [COMMAND_OPTIONS]\n\n"
            "List of commands:\n"
            "  help     Prints help for commands.\n"
	    "  list     List files inside package files.\n"
            "  convert  Converts game files into their encrypted/decrypted\n"
            "           counterpart.\n"
            "  merge    Merge many kind of files.\n"
            "  pack     Pack game files into package files.\n\n"

            "For help on a command use: %s help <command>\n"
            , bin, bin);
    }
}

bool ShadyCli::Command::run(int argc, char* argv[]) {
    ShadyCli::Command* command = 0;
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        printHelp(argv[0], argc >= 3 ? getCommand(argv[2]) : 0);
        return false;
    } else {
        command = getCommand(argv[1]);
    }

    if (command) {
        command->buildOptions();
        auto result = command->options.parse(--argc, argv = &argv[1]);
        auto ret = command->run(result);
        delete command;
        return ret;
    } else {
        printf("Unknown command: %s. Use `%s help` for help.\n", argv[1], argv[0]);
    }

    return false;
}

namespace ShadyCore {
    void* Allocate(size_t s) { return new uint8_t[s]; }
    void Deallocate(void* p) { delete[] (uint8_t*)p; }
}

int main(int argc, char* argv[]) {
    if (ShadyCli::Command::run(argc, argv)) return 0;
    return 1;
}
