#pragma once
#include "baseentry.hpp"
#include <stack>

namespace ShadyCore {
	class ZipStream : public std::streambuf {
	private:
		void *pkgFile = 0;
		void *innerFile = 0;
		unsigned int index;
		std::stack<int_type> pool;
		std::streamoff pos = 0;
		std::streamsize size;
	protected:
		// DISALLOWED
		int_type pbackfail(int_type) override { throw; }
		void imbue(const std::locale&) override { throw; }
		std::streambuf* setbuf(char_type*, std::streamsize) override { throw; }
		std::streamsize xsputn(const char_type*, std::streamsize) override { throw; }
		int_type overflow(int_type value) override { throw; }

		pos_type seekoff(off_type, std::ios::seekdir, std::ios::openmode) override;
		pos_type seekpos(pos_type, std::ios::openmode) override;
		std::streamsize showmanyc() override { return size - pos; }

		std::streamsize xsgetn(char_type*, std::streamsize) override;
		int_type underflow() override;
		int_type uflow() override;
	public:
		inline bool isOpen() const { return pkgFile; }
		void open(const char*, const char*);
		void close();
	};

	class ZipPackageEntry : public BasePackageEntry {
	private:
		const char* packageFilename;

		ZipStream zipBuffer;
		std::istream zipStream;
	public:
		inline ZipPackageEntry(int id, const char* packageFilename, const char* name, unsigned int size)
			: BasePackageEntry(id, name, size), packageFilename(packageFilename), zipStream(&zipBuffer) {}

		inline EntryType getType() const override final { return TYPE_ZIP; }
		inline bool isOpen() const override final { return zipBuffer.isOpen(); }
		inline std::istream& open() override final { zipStream.clear(); zipBuffer.open(packageFilename, name); return zipStream; }
		inline void close() override final { zipBuffer.close(); }
	};
}
