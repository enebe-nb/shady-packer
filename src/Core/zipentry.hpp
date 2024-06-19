#pragma once

#include "baseentry.hpp"
#include "package.hpp"
#include <stack>
#include <deque>

namespace ShadyCore {
	class ZipStream : public std::streambuf {
	private:
		void *pkgFile = 0;
		void *innerFile = 0;
		unsigned int index;
		std::streamoff pos = 0;
		std::streamsize size;
		int_type bufVal;
		bool hasBufVal = false;

	protected:
		int_type pbackfail(int_type) override { throw std::exception("Disallowed method."); }
		void imbue(const std::locale&) override { throw std::exception("Disallowed method."); }
		std::streambuf* setbuf(char_type*, std::streamsize) override { throw std::exception("Disallowed method."); }
		std::streamsize xsputn(const char_type*, std::streamsize) override { throw std::exception("Disallowed method."); }
		int_type overflow(int_type value) override { throw std::exception("Disallowed method."); }

		pos_type seekoff(off_type, std::ios::seekdir, std::ios::openmode) override;
		pos_type seekpos(pos_type, std::ios::openmode) override;
		std::streamsize showmanyc() override { return size - pos; }

		std::streamsize xsgetn(char_type*, std::streamsize) override;
		int_type underflow() override;
		int_type uflow() override;
	public:
		inline bool isOpen() const { return pkgFile; }
		void open(const std::filesystem::path&, const std::string&);
		void close();
	};

	class ZipPackageEntry : public BasePackageEntry {
	private:
		const std::string name;

	public:
		inline ZipPackageEntry(Package* parent, const std::string& name, unsigned int size)
			: BasePackageEntry(parent, size), name(name) {}

		inline StorageType getStorage() const override final { return TYPE_ZIP; }
		std::istream& open() override final;
		void close(std::istream&) override final;
	};

	ShadyCore::FileType GetZipPackageDefaultType(const ShadyCore::FileType& inputType, ShadyCore::BasePackageEntry* entry);
}
