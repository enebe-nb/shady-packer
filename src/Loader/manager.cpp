#include <iostream>
#include <filesystem>
#include <fstream>
#include <termcolor/termcolor.hpp>
#include "modpackage.hpp"
// NOTE: Don't use wchar is this file, so it can be build on linux
// TODO find replacement for PrivateProfile on linux

void saveJson() {
    nlohmann::json root;
	for (auto& package : ModPackage::packageList) {
		root[package->name.string()] = package->data;
	}

	std::ofstream output(ModPackage::basePath / "packages.json");
	output << root;
}

void showLoading(AsyncTask& task) {
    std::cout << '-' << std::flush;
    for(;;) {
        if (task.isDone()) break;
        std::cout << "\b\\" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (task.isDone()) break;
        std::cout << "\b|" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (task.isDone()) break;
        std::cout << "\b/" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (task.isDone()) break;
        std::cout << "\b-" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void showIntro() {
    std::cout << termcolor::green << "This is a Mod Manager for shady-loader." << std::endl;
    std::cout << termcolor::reset;
    std::cout << "It's made this way because is faster to create and gives less problems." << std::endl;
    std::cout << "Please bear with this for now." << std::endl;
    std::cout << "If you block this program on your firewall you won't be able to download mods." << std::endl << std::endl;
}

bool enterViewMod(ModPackage* package) {
    std::cout << std::endl << "Name: "<< termcolor::green << package->name << termcolor::reset << std::endl;
    std::cout << "Version: " << package->version() << std::endl;
    std::cout << "Creator: " << package->creator() << std::endl;
    std::cout << "Description: " << package->description() << std::endl << std::endl;

    std::cout << "Tags: ";
    for (auto& tag : package->tags) {
        std::cout << tag << "; ";
    } std::cout << std::endl << std::endl;

    std::vector<char> options;
    options.push_back('B');
    std::cout << "Local Package: " << (package->isLocal() ? termcolor::green : termcolor::red)
        << (package->isLocal() ? "true" : "false") << termcolor::reset << std::endl;
    std::cout << "Status: ";
    if (package->fileExists) {
        options.push_back(package->enabled ? 'D' : 'E');
        if (package->requireUpdate) {
            options.push_back('U');
            std::cout << termcolor::yellow << "Found Updates" << termcolor::reset << std::endl;
        } else {
            std::cout << termcolor::green << "Updated" << termcolor::reset << std::endl;
        }
        std::cout << "Enabled: " << (package->enabled ? termcolor::green : termcolor::red)
            << (package->enabled ? "true" : "false") << termcolor::reset << std::endl;
    } else {
        options.push_back('O');
        std::cout << termcolor::red << "Not Downloaded" << termcolor::reset << std::endl;
    }

    std::cout << "Choose an option (" << termcolor::yellow;
    for (char c : options) {
        switch (c) {
        case 'E': std::cout << "[E]nable; "; break;
        case 'D': std::cout << "[D]isable; "; break;
        case 'U': std::cout << "[U]pdate; "; break;
        case 'O': std::cout << "d[O]wnload; "; break;
        case 'B': std::cout << "[B]ack/cancel; "; break;
        }
    } std::cout << "\b\b" << termcolor::reset << "): ";
    char choice; std::cin >> choice;
    choice = std::toupper(choice);
    if (std::find(options.begin(), options.end(), choice) == options.end()) {
        std::cout << std::endl << "Invalid Options" << std::endl;
        return true;
    }

    switch (choice) {
        case 'E':
        case 'D':
            package->enabled = !package->enabled;
            WritePrivateProfileStringA("Packages", package->name.string().c_str(), package->enabled ? "1" : "0",
                (ModPackage::basePath / "shady-loader.ini").string().c_str());
        break;
        case 'U':
        case 'O': {
            package->downloadFile();
            showLoading(*package->downloadTask);
            std::cout << std::endl;

            std::filesystem::path filename(ModPackage::basePath / package->name);
            filename += package->ext; filename += ".part";
            if (std::filesystem::exists(filename)) {
                std::filesystem::path target(filename);
                target.replace_extension();
                std::filesystem::rename(filename, target);
                std::cout << termcolor::green << "Download Succeeded" << termcolor::reset << std::endl;
                package->fileExists = true;
                package->requireUpdate = false;
                package->data["version"] = package->data.value("remoteVersion", "");
                saveJson();
            } else {
                std::cout << termcolor::red << "Download Failed" << termcolor::reset << std::endl;
            }
            delete package->downloadTask;
            package->downloadTask = 0;
        } break;
        case 'B': return false;
    }

    return true;
}

bool enterManage2() {
    std::cout << "List of Mods:" << std::endl;
    std::cout << termcolor::green << " [1] Return to previous menu" << termcolor::reset << std::endl;
    int index = 1;
    for (auto& package : ModPackage::packageList) {
        std::cout << (package->enabled ? termcolor::green : termcolor::yellow)
            << " [" << ++index << "] " << package->name
            << termcolor::reset << std::endl;
    }
    
    std::cout << termcolor::reset << std::endl << "Choose a Mod [1-" << index << "]: ";
    int option; std::cin >> option;

    if (option == 1) {
        return false;
    } else if (option > 1 && option <= index) {
        while(enterViewMod(ModPackage::packageList[option - 2]));
    } else {
        std::cout << "Invalid option" << std::endl;
    }

    return true;
}

void enterManage() {
    std::cout << "Parsing local mods..." << std::endl;
    for (auto& package : ModPackage::packageList) delete package;
	ModPackage::packageList.clear();
    ModPackage::LoadFromLocalData();
    ModPackage::LoadFromFilesystem();
    for (auto& package : ModPackage::packageList) {
		package->enabled = GetPrivateProfileIntA("Packages", package->name.string().c_str(), false,
            (ModPackage::basePath / "shady-loader.ini").string().c_str());
	}

    std::cout << "Checking mods database ";
    FetchJson remoteConfig("1EpxozKDE86N3Vb8b4YIwx798J_YfR_rt");
    remoteConfig.start();
    showLoading(remoteConfig);
    std::cout << std::endl;

    ModPackage::LoadFromRemote(remoteConfig.data);
    saveJson();

    while(enterManage2());
}

bool enterExtra() {
    int useIntercept = GetPrivateProfileIntA("Options", "useIntercept", false,
        (ModPackage::basePath / "shady-loader.ini").string().c_str());
    int autoUpdate = GetPrivateProfileIntA("Options", "autoUpdate", true, 
        (ModPackage::basePath / "shady-loader.ini").string().c_str());
    int useLoadLock = GetPrivateProfileIntA("Options", "useLoadLock", true,
        (ModPackage::basePath / "shady-loader.ini").string().c_str());

    std::cout << termcolor::reset << "List of extra options:" << std::endl;
    std::cout << (useIntercept ? termcolor::green : termcolor::red)
        << " [1] Use Intercept" << termcolor::reset
        << " - This replaces code in SokuEngine, may cause problems" << std::endl;
    std::cout << (autoUpdate ? termcolor::green : termcolor::red)
        << " [2] Auto Update" << termcolor::reset
        << " - Auto updates mods on game start" << std::endl;
    std::cout << (useLoadLock ? termcolor::green : termcolor::red)
        << " [3] Use Load Lock" << termcolor::reset
        << " - Prevents the game to start before all mods loads." << std::endl;
    std::cout << termcolor::green << " [4] " << termcolor::reset << "Return"<< std::endl;

    std::cout << std::endl << "Choose a option [1-4]: ";
    int option; std::cin >> option;

    switch(option) {
        case 1:
            useIntercept = !useIntercept;
            WritePrivateProfileStringA("Options", "useIntercept", useIntercept ? "1" : "0",
                (ModPackage::basePath / "shady-loader.ini").string().c_str());
            return true;
        case 2:
            autoUpdate = !autoUpdate;
            WritePrivateProfileStringA("Options", "autoUpdate", autoUpdate ? "1" : "0",
                (ModPackage::basePath / "shady-loader.ini").string().c_str());
            return true;
        case 3:
            useLoadLock = !useLoadLock;
            WritePrivateProfileStringA("Options", "useLoadLock", useLoadLock ? "1" : "0", 
                (ModPackage::basePath / "shady-loader.ini").string().c_str());
            return true;
        case 4:
            return false;
        default:
            std::cout << "Invalid Option" << std::endl;
            return true;
    }
}

bool enterHowto() {
    std::cout << "This needs improvement but basically:" << std::endl;
    std::cout << "1 - Create a folder or a zip file with the mod name in the shady-loader folder." << std::endl;
    std::cout << "2 - Put the soku modified files inside it." << std::endl;
    std::cout << "done, the loader will find it and you can enabled it with this." << std::endl << std::endl;
    return false;
}

bool enterMain() {
    std::cout << termcolor::reset << "List of options:" << std::endl;
    std::cout << termcolor::green << " [1] " << termcolor::reset
        << "Manage modules" << std::endl;
    std::cout << termcolor::green << " [2] " << termcolor::reset
        << "Edit extra options" << std::endl;
    std::cout << termcolor::green << " [3] " << termcolor::reset
        << "How to create custom mods" << std::endl;
    std::cout << termcolor::green << " [4] " << termcolor::reset
        << "Exit this application" << std::endl;
    std::cout << std::endl << "Choose a option [1-4]: ";
    int option; std::cin >> option;

    switch(option) {
        case 1:
            enterManage();
            return true;
        case 2:
            while(enterExtra());
            return true;
        case 3:
            while(enterHowto());
            return true;
        case 4:
            return false;
        default:
            std::cout << "Invalid Option" << std::endl;
            return true;
    }
}

int main () {
    showIntro();
    while(enterMain());
    return 0;
}