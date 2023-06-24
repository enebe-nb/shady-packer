#pragma once
#include "baseentry.hpp"
#include "package.hpp"
#include <stack>

namespace ShadyCore {
	class StreamFilter : public std::streambuf {
	protected:
		std::streambuf* base;
		std::streamoff pos = 0;
		std::streamsize size;
		int_type bufVal;
		bool hasBufVal = false;

		virtual char_type filter(char_type) = 0;
		virtual void seek(off_type, std::ios::seekdir) = 0;
	public:
		inline StreamFilter(std::streambuf* base, std::streamsize size) : base(base), size(size) {};

		// DISALLOWED
		int_type pbackfail(int_type) override { throw; }
		void imbue(const std::locale&) override { throw; }
		std::streambuf* setbuf(char_type*, std::streamsize) override { throw; }
		std::streamsize showmanyc() override { throw; }

		pos_type seekoff(off_type, std::ios::seekdir, std::ios::openmode) override;
		pos_type seekpos(pos_type, std::ios::openmode) override;

		std::streamsize xsgetn(char_type*, std::streamsize) override;
		int_type underflow() override;
		int_type uflow() override;
		std::streamsize xsputn(const char_type*, std::streamsize) override;
		int_type overflow(int_type) override;
		int sync() override { return base->pubsync(); }
	};

	class DataListFilter : public StreamFilter {
	private:
		static const unsigned short TABLE_SIZE = 624;
		static const unsigned short TABLE_SHIFT = 397;
		static const unsigned long MATRIX_A = 0x9908b0dfUL;
		static const unsigned long UPPER_MASK = 0x80000000UL;
		static const unsigned long LOWER_MASK = 0x7fffffffUL;

		unsigned long table[TABLE_SIZE];
		unsigned long seed;
		int index;
		int key[2];

		void init();
		char_type randValue();
		char_type filter(char_type value) override;
		void seek(off_type, std::ios::seekdir) override;
	public:
		inline DataListFilter(std::streambuf* base, unsigned long seed, std::streamsize size) : StreamFilter(base, size), seed(seed) { init(); }
	};

	class DataFileFilter : public StreamFilter {
	private:
		unsigned int offset;
		int key;

		inline char_type filter(char_type value) override final { return value ^ key; }
		void seek(off_type, std::ios::seekdir) override;
	public:
		DataFileFilter(std::streambuf* base, unsigned int offset, unsigned int size)
			: StreamFilter(base, size), offset(offset) { key = (offset >> 1) | 0x23; }
		//inline DataFileFilter(unsigned int offset, unsigned int size) : DataFileFilter(0, offset, size) {};
	};

	class DataPackageEntry : public BasePackageEntry {
	private:
		unsigned int packageOffset;
		unsigned int packageSize;
		// std::istream fileStream;
		// DataFileFilter fileFilter;

	public:
		inline DataPackageEntry(Package* parent, unsigned int offset, unsigned int size)
			//: BasePackageEntry(parent, size), fileStream(&fileFilter), fileFilter(offset, size), packageOffset(offset) {}
			: BasePackageEntry(parent, size), packageOffset(offset), packageSize(size) {}

		inline StorageType getStorage() const override final { return TYPE_DATA; }
		//inline bool isOpen() const override final { return fileFilter.getBaseBuffer(); }
		std::istream& open() override;
		void close(std::istream&) override;
	};

	ShadyCore::FileType GetDataPackageDefaultType(const ShadyCore::FileType& inputType, ShadyCore::BasePackageEntry* entry);
	ShadyCore::FileType GetDataPackageDefaultType(const ShadyCore::FileType::Type& inputType, ShadyCore::Resource* resource);
}