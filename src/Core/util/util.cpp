#include "riffdocument.hpp"
#include "xmlprinter.hpp"
#include <list>
#include <stack>
#include <vector>

ShadyUtil::RiffDocument::RiffDocument(std::istream& input) : input(input) {
    typedef struct{ char type[4]; uint32_t left; } ListInfo;
	char buffer[4]; uint32_t offset = 0;
    std::vector<ListInfo> stack;

    do {
        if (input.read(buffer, 4).gcount() != 4) break;
        offset += 8;

        if (strncmp(buffer, "RIFF", 4) == 0 || strncmp(buffer, "LIST", 4) == 0) {
            stack.emplace_back();
            input.read((char*)&stack.back().left, 4);
            input.read(stack.back().type, 4);
            stack.back().left -= 4;
            offset += 4;
        } else {
            uint32_t size;
            input.read((char*)&size, 4);
            input.ignore(size);

            char* key = new char[stack.size() * 4 + 5]; int i;
            for (i = 0; i < stack.size(); ++i) memcpy(key + i * 4, stack[i].type, 4);
            memcpy(key + i++ * 4, buffer, 4);
            key[i * 4] = '\0';

            chunks[key] = DataType{ offset, size };

            stack.back().left -= size + 8;
            offset += size;
        }

        if (stack.back().left <=0 ) stack.pop_back();
    } while(stack.size());

	input.clear();
}

void ShadyUtil::RiffDocument::read(const char* key, uint8_t* buffer, uint32_t offset, uint32_t size) {
    auto i = chunks.find(key);
    if (!size) size = i->second.size;
    input.seekg(i->second.offset + offset);
    input.read((char*)buffer, size);
}

void ShadyUtil::XmlPrinter::openNode(const char* name) {
	char* nName = new char[strlen(name) + 1];
	strcpy(nName, name);
	stack.push(nName);

	if (isOpened) output << '>';
	output << '\n';
	for (uint32_t i = 0; i < indent; ++i) output << '\t';
	output << "<" << nName;
	isOpened = true;
	++indent;
}

void ShadyUtil::XmlPrinter::appendAttribute(const char* name, const char* value) {
	if (!isOpened) throw;

	output << " " << name << "=\"";
	while (*value) {
		switch (*value) {
		case '<': output << "&lt;"; break;
		case '>': output << "&gt;"; break;
		case '"': output << "&quot;"; break;
		case '&': output << "&amp;"; break;
		default: output << *value; break;
		}
		++value;
	}
	output << '"';
}

void ShadyUtil::XmlPrinter::appendContent(const char* content) {
	if (isOpened) {
		output << '>';
		isOpened = false;
	}
	
	while (content) {
		switch (*content) {
		case '<': output << "&lt;"; break;
		case '>': output << "&gt;"; break;
		case '&': output << "&amp;"; break;
		default: output << *content; break;
		}
		++content;
	}
}

void ShadyUtil::XmlPrinter::closeNode() {
	--indent;
	if (isOpened) output << "/>";
	else {
		output << '\n';
		for (uint32_t i = 0; i < indent; ++i) output << '\t';
		output << "</" << stack.top() << ">";
	}
	delete[] stack.top();
	stack.pop();
	isOpened = false;
}

#ifdef _WIN32

#include "filewatcher.hpp"
#include <windows.h>
#include <mutex>

namespace {
	struct Change {
		ShadyUtil::FileWatcher* watcher;
		ShadyUtil::FileWatcher::Action action;
		inline bool operator==(const Change& o) { return watcher == o.watcher && action == o.action; }
	};
	std::list<Change> changes;

	const size_t bufferSize = 32*1024;
	struct Delegate {
		HANDLE handle;
		OVERLAPPED overlapped = {0};
		uint8_t buffer[bufferSize];
		std::filesystem::path folder;
		std::list<ShadyUtil::FileWatcher*> files;

		inline std::wstring _trunk(const std::wstring_view& filename) {
			size_t len = filename.find_first_of(L"\\/");
			if (len == std::string_view::npos) return std::wstring(filename);
			return std::wstring(filename.substr(0, len));
		}

		inline ShadyUtil::FileWatcher* _find(const std::wstring_view& filename) {
			for (auto file : files) if (file->filename == filename) return file;
			return 0;
		}

		inline void _push_unique(const Change& newchange) {
			for (auto& change : changes) if (change == newchange) return;
			changes.push_back(newchange);
		}

		static inline Delegate* create(const std::filesystem::path& folder) {
			HANDLE handle = CreateFileW(folder.c_str(), FILE_LIST_DIRECTORY,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
			if (handle == INVALID_HANDLE_VALUE) return 0;

			auto delegate = new Delegate;
			delegate->handle = handle;
			delegate->overlapped.hEvent = CreateEvent(nullptr, true, false, nullptr);
			if (!delegate->overlapped.hEvent) throw std::runtime_error("Fail to create a event " + std::to_string(GetLastError())); // TODO
			if (!ReadDirectoryChangesW(handle, delegate->buffer, bufferSize, true,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
				nullptr, &delegate->overlapped, nullptr)) throw std::runtime_error("Fail to read directory (start) " + std::to_string(GetLastError())); // TODO
			delegate->folder = folder;

			return delegate;
		}

		inline ~Delegate() { // TODO better cancel
			DWORD bytes;
			CancelIoEx(handle, &overlapped);
			GetOverlappedResult(handle, &overlapped, &bytes, true);
			CloseHandle(overlapped.hEvent);
			CloseHandle(handle);
		}

		void doHandle() {
			DWORD bytes;
			if (GetOverlappedResult(handle, &overlapped, &bytes, true)) {
				FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
				std::wstring_view oldFilename;
				if (bytes) for(;;) {
					std::wstring filename = _trunk(std::wstring_view(info->FileName, info->FileNameLength/sizeof(WCHAR)));

					ShadyUtil::FileWatcher* watcher;
					switch (info->Action) {
					case FILE_ACTION_ADDED:
					case FILE_ACTION_REMOVED:
					case FILE_ACTION_MODIFIED:
						if (watcher = _find(filename)) _push_unique({watcher, (ShadyUtil::FileWatcher::Action)info->Action});
						break;
					case FILE_ACTION_RENAMED_OLD_NAME:
						oldFilename = filename;
						break;
					case FILE_ACTION_RENAMED_NEW_NAME:
						if (watcher = _find(oldFilename)) {
							watcher->filename = filename;
							_push_unique({watcher, ShadyUtil::FileWatcher::RENAMED});
						} else if (watcher = _find(filename)) {
							_push_unique({watcher, ShadyUtil::FileWatcher::CREATED});
						} break;
					}

					if (info->NextEntryOffset == 0) break;
					else info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>((ptrdiff_t)info + info->NextEntryOffset);
				}
			}

			if (!ReadDirectoryChangesW(handle, buffer, bufferSize, true,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
				nullptr, &overlapped, nullptr)) throw std::runtime_error("Fail to read directory (loop) " + std::to_string(GetLastError())); // TODO
		};

		inline bool empty() { return files.empty(); };
		inline bool contains(ShadyUtil::FileWatcher* f) { for (auto p : files) if (p == f) return true; return false; };
		inline void add(ShadyUtil::FileWatcher* f) { files.push_back(f); };
		inline void remove(ShadyUtil::FileWatcher* f) { files.remove(f); };
		inline bool isFolder(const std::filesystem::path& path) { return std::filesystem::equivalent(folder, path); };
	};

	std::vector<HANDLE> handles;
	std::vector<Delegate*> delegates;
	std::mutex delegateMutex;
}

ShadyUtil::FileWatcher* ShadyUtil::FileWatcher::create(const std::filesystem::path& path) {
	ShadyUtil::FileWatcher* watcher;
	std::filesystem::path folder = path.parent_path();
	std::lock_guard lock(delegateMutex);

	for (int i = 0; i < delegates.size(); ++i) {
		if (delegates[i]->isFolder(folder)) {
			delegates[i]->add(watcher = new FileWatcher(path.filename()));
			return watcher;
		}
	}

	auto delegate = Delegate::create(folder);
	if (delegate) {
		delegate->add(watcher = new FileWatcher(path.filename()));
		delegates.push_back(delegate);
		handles.push_back(delegate->overlapped.hEvent);
		return watcher;
	}

	return 0;
}

ShadyUtil::FileWatcher::~FileWatcher() {
	std::lock_guard lock(delegateMutex);

	for (int i = 0; i < delegates.size();) {
		if (delegates[i]->contains(this)) delegates[i]->remove(this);

		if (delegates[i]->empty()) {
			delete delegates[i];
			handles.erase(handles.begin() + i);
			delegates.erase(delegates.begin() + i);
		} else ++i;
	}
}

ShadyUtil::FileWatcher* ShadyUtil::FileWatcher::getNextChange() {
	std::unique_lock lock(delegateMutex, std::try_to_lock);
	if (!lock.owns_lock()) return 0;

	if (changes.empty() && handles.size()) {
		DWORD result = WaitForMultipleObjects(handles.size(), handles.data(), false, 0);
		if (result == WAIT_FAILED) throw std::runtime_error("FileWatcher wait failed: " + std::to_string(GetLastError())); // TODO
		if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + handles.size()) {
			delegates[result - WAIT_OBJECT_0]->doHandle();
		}
	} if (changes.empty()) return 0;

	auto watcher = changes.front().watcher;
	watcher->action = changes.front().action;
	changes.pop_front();
	return watcher;
}

#endif /* _WIN32 */
