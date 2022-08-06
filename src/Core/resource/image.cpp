#include "readerwriter.hpp"
#include <png.h>
#include <math.h>
#include <string>

namespace {
	bool isTrueColorAvailable = false;
}

void ShadyCore::Palette::setTrueColorAvailable(bool value) {
	isTrueColorAvailable = value;
}

uint16_t ShadyCore::Palette::packColor(uint32_t color, bool transparent) {
	uint16_t pcolor = !transparent;
	pcolor = (pcolor << 5) + ((color >> 19) & 0x1F);
	pcolor = (pcolor << 5) + ((color >> 11) & 0x1F);
	pcolor = (pcolor << 5) + ((color >> 3) & 0x1F);
	return pcolor;
}

uint32_t ShadyCore::Palette::unpackColor(uint16_t color) {
	uint32_t ucolor;

	ucolor = (color >> 7) & 0xF8;
	ucolor += (ucolor >> 5) & 0x07;
	ucolor = ucolor << 8;

	ucolor += (color >> 2) & 0xF8;
	ucolor += (ucolor >> 5) & 0x07;
	ucolor = ucolor << 8;

	ucolor += (color << 3) & 0xF8;
	ucolor += (ucolor >> 5) & 0x07;

	if (color & 0x8000) ucolor |= 0xff000000;
	return ucolor;
}

void ShadyCore::Palette::pack() {
	if (bitsPerPixel <= 16) return;

	uint16_t* newData = (uint16_t*)ShadyCore::Allocate(256 * 2);
	for (int i = 0; i < 256; ++i) {
		uint32_t* _data = (uint32_t*)data;
		newData[i] = packColor(_data[i]);
	}

	destroy();
	bitsPerPixel = 16;
	data = (uint8_t*)newData;
}

void ShadyCore::Palette::unpack() {
	if (bitsPerPixel > 16) return;

	uint32_t* newData = (uint32_t*)ShadyCore::Allocate(256 * 4);
	for (int i = 0; i < 256; ++i) {
		uint16_t* _data = (uint16_t*)data;
		newData[i] = unpackColor(_data[i]);
	}

	destroy();
	bitsPerPixel = 32;
	data = (uint8_t*)newData;
}

static void pngRead(png_structp pngData, png_bytep buffer, png_size_t length) {
	std::istream* input = (std::istream*) png_get_io_ptr(pngData);
	input->read((char*)buffer, length);
}

static void pngWrite(png_structp pngData, png_bytep buffer, png_size_t length) {
	std::ostream* output = (std::ostream*) png_get_io_ptr(pngData);
	output->write((char*)buffer, length);
}

static void pngFlush(png_structp pngData) {
	std::ostream* output = (std::ostream*) png_get_io_ptr(pngData);
	output->flush();
}

static void pngError(png_structp pngData, png_const_charp message) {
	printf("%s\n", message);
	throw;
}

static void pngWarning(png_structp pngData, png_const_charp message) {
	printf("%s\n", message);
}

const static png_color emptyPalette[256 * 3] = { 0 };

namespace _private {
void readerImagePng(ShadyCore::Image& resource, std::istream& input) {
	png_structp pngData = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, pngError, pngWarning);
	png_infop pngInfo = png_create_info_struct(pngData);
	png_set_read_fn(pngData, &input, pngRead);
	png_read_info(pngData, pngInfo);

	// Read Info
	resource.width = png_get_image_width(pngData, pngInfo);
	resource.height = png_get_image_height(pngData, pngInfo);
	resource.paddedWidth = (resource.width + 3) / 4 * 4;

	// Register transformations
	int depth = png_get_bit_depth(pngData, pngInfo);
	if (depth > 8) png_set_strip_16(pngData);
	if (depth < 8) png_set_packing(pngData);
	int colorType = png_get_color_type(pngData, pngInfo);
	if (colorType & PNG_COLOR_MASK_PALETTE) {
		resource.bitsPerPixel = 8;
	} else {
		resource.bitsPerPixel = colorType & PNG_COLOR_MASK_ALPHA ? 32 : 24;
		if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(pngData);
		if (!(colorType & PNG_COLOR_MASK_ALPHA)) png_set_filler(pngData, 0xff, PNG_FILLER_AFTER);
		png_set_bgr(pngData);
	} png_read_update_info(pngData, pngInfo);
	resource.altSize = 0;
	resource.initialize();
	memset(resource.raw, 0, resource.getRawSize());

	// Read Image
	//int rowSize = png_get_rowbytes(pngData, pngInfo);
	int rowSize = resource.paddedWidth * (resource.bitsPerPixel == 8 ? 1 : 4);
	uint8_t ** pointers = new uint8_t*[resource.height];
	for (int i = 0; i < resource.height; ++i) {
		pointers[i] = resource.raw + i * rowSize;
	} png_read_image(pngData, (png_bytepp)pointers);
	delete[] pointers;

	// Finish
	png_read_end(pngData, 0);
	png_destroy_read_struct(&pngData, &pngInfo, 0);
}

void readerImageCv(ShadyCore::Image& resource, std::istream& input) {
	input.read((char*)&resource.bitsPerPixel, 1);
	input.read((char*)&resource.width, 16); // , height, paddedWidth, altSize

	resource.initialize();
	input.read((char*)resource.raw, resource.getRawSize());
}

void readerImageBmp(ShadyCore::Image& resource, std::istream& input) {
	uint16_t magicNumber; input.read((char*)&magicNumber, 2);
	if (magicNumber != 0x4d42) return;
	input.ignore(8);
	uint32_t dataOffset; input.read((char*)&dataOffset, 4);
	uint32_t headerSize; input.read((char*)&headerSize, 4);
	if (headerSize < 16) return;

	uint16_t bitsPerPixel;
	input.read((char*)&resource.width, 8); // , height
	input.ignore(2);
	input.read((char*)&bitsPerPixel, 2);
	resource.paddedWidth = (resource.width + 3) / 4 * 4;

	uint32_t compressionMethod, dataSize = 0;
	size_t rowSize = (bitsPerPixel * resource.width + 31) / 32 * 4;
	if (headerSize > 16) {
		input.read((char*)&compressionMethod, 4);
		input.read((char*)&dataSize, 4);
	} if (!dataSize) {
		compressionMethod = 0;
		dataSize = rowSize * resource.height;
	}

	resource.altSize = 0;
	input.seekg(dataOffset);
	if (bitsPerPixel <= 8) {
		resource.bitsPerPixel = 8;
		resource.initialize();
		if (compressionMethod) throw std::runtime_error("Compressed Bitmap is not implemented"); // TODO
	} else if (bitsPerPixel >= 24) {
		resource.bitsPerPixel = 32;
		resource.initialize();
	} else {
		resource.bitsPerPixel = 16;
		resource.initialize();
	}

	if (bitsPerPixel != resource.bitsPerPixel) {
		const auto target = resource.raw;
		uint8_t* buffer = new uint8_t[dataSize];
		input.read((char*)buffer, dataSize);

		if (bitsPerPixel < 8) {
			const int mask = (1 << bitsPerPixel) - 1;
			int bitOffset = 0;
			// TODO fix inverted Y axis
			int i = 0; while (i < rowSize*resource.height) {
				target[i*8/bitsPerPixel] = (buffer[i] >> bitOffset) & mask;

				bitOffset += bitsPerPixel;
				if (bitOffset >= 8) { ++i; bitOffset = 0; }
			}
		} else {
			for (int j = 0; j < resource.height; ++j) {
				const int J = (resource.height - j - 1);
				for (int i = 0; i < resource.width; ++i) {
					target[J*resource.width + i*4]   = buffer[j*rowSize + i*3];
					target[J*resource.width + i*4+1] = buffer[j*rowSize + i*3+1];
					target[J*resource.width + i*4+2] = buffer[j*rowSize + i*3+2];
					target[J*resource.width + i*4+3] = 0xff;
				}
			}
		}

		delete buffer;
	} else {
		const int rawRowSize = (resource.bitsPerPixel == 24 ? 4 : resource.bitsPerPixel/8) * resource.paddedWidth;
		for (int i = 0; i < resource.height; ++i) {
			input.read((char*)&resource.raw[(resource.height - i - 1)*rawRowSize], rowSize);
		}
	}
}

void writerImagePng(ShadyCore::Image& resource, std::ostream& output) {
	// Initialize
	png_structp pngData = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, pngError, pngWarning);
	png_infop pngInfo = png_create_info_struct(pngData);
	png_set_write_fn(pngData, &output, pngWrite, pngFlush);

	// Setup Info
	int colorType
		= resource.bitsPerPixel == 8 ? PNG_COLOR_TYPE_PALETTE
		: resource.bitsPerPixel == 24 ? PNG_COLOR_TYPE_RGB
		: PNG_COLOR_TYPE_RGB_ALPHA;
	png_set_IHDR(pngData, pngInfo, resource.width, resource.height, 8, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	if (colorType == PNG_COLOR_TYPE_PALETTE) {
		png_set_PLTE(pngData, pngInfo, emptyPalette, 256);
	}

	png_write_info(pngData, pngInfo);
	if (colorType == PNG_COLOR_TYPE_RGB) png_set_filler(pngData, 0, PNG_FILLER_AFTER);
	if (colorType != PNG_COLOR_TYPE_PALETTE) png_set_bgr(pngData);

	// Write Image
	if (resource.bitsPerPixel == 16) throw std::runtime_error("Writing 16bits Png is not supported");
	int rowSize = resource.paddedWidth * (resource.bitsPerPixel == 8 ? 1 : 4);
	const uint8_t ** pointers = new const uint8_t*[resource.height];
	for (int i = 0; i < resource.height; ++i) {
		pointers[i] = resource.raw + i * rowSize;
	} png_write_image(pngData, (png_bytepp)pointers);
	delete[] pointers;

	// Finish
	png_write_end(pngData, pngInfo);
	png_destroy_write_struct(&pngData, &pngInfo);
}

void writerImageCv(ShadyCore::Image& resource, std::ostream& output) {
	output.write((char*)&resource.bitsPerPixel, 1);
	output.write((char*)&resource.width, 16); // , height, paddedWidth, altSize

	output.write((char*)resource.raw, resource.getRawSize());
}

void writerImageBmp(ShadyCore::Image& resource, std::ostream& output) {
	const size_t rowSize = (resource.bitsPerPixel * resource.width + 31) / 32 * 4;
	output.write("BM", 2);
	uint32_t offset = resource.bitsPerPixel == 8 ? 265*4 : 0;
	offset += 54 + rowSize * resource.height;
	output.write((char*)&offset, 4);
	output.write("\0\0\0\0", 4);
	offset -= rowSize * resource.height;
	output.write((char*)&offset, 4);

	offset = 40; output.write((char*)&offset, 4);
	output.write((char*)&resource.width, 8); // , height
	output.write("\1\0", 2);
	output.put(resource.bitsPerPixel);
	for (int i = 0; i < 25; ++i) output.put('\0');

	if (resource.bitsPerPixel == 8) {
		for (int i = 0; i < 256; ++i) output.write("\0\0\0\0", 4);
	}

	for (int i = 0; i < resource.height; ++i) {
		offset = (resource.height - i - 1) * resource.paddedWidth * (resource.bitsPerPixel == 24 ? 4 : resource.bitsPerPixel/8);
		output.write((char*)&resource.raw[offset], rowSize);
	}
}

void readerPaletteAct(ShadyCore::Palette& resource, std::istream& input) {
	if (isTrueColorAvailable) for (int i = 0; i < 256; ++i) {
		resource.initialize(32);
		input.read((char*)&resource.data[i*4], 3);
		resource.data[i*4+3] = i == 0 ? 0 : 0xff;
	} else {
		resource.initialize(16);
		for (int i = 0; i < 256; ++i) {
			uint32_t color = 0;
			uint16_t* data = (uint16_t*)resource.data;
			input.read((char*)&color, 3);
			data[i] = ShadyCore::Palette::packColor(color, i == 0);
		}
	}
}

void readerPalette(ShadyCore::Palette& resource, std::istream& input) {
	uint8_t bitsPerPixel;
	input.read((char*)&bitsPerPixel, 1);
	resource.initialize(bitsPerPixel);
	input.read((char*)resource.data, resource.getDataSize());
}

void writerPaletteAct(ShadyCore::Palette& resource, std::ostream& output) {
	if (resource.bitsPerPixel <= 16) {
		for (int i = 0; i < 256; ++i) {
			uint16_t* data = (uint16_t*)resource.data;
			uint32_t color = resource.unpackColor(data[i]);
			output.write((char*)&color, 3);
		}
	} else {
		for (int i = 0; i < 256; ++i) {
			uint32_t* data = (uint32_t*)resource.data;
			output.write((char*)&data[i], 3);
		}
	}
}

void writerPalette(ShadyCore::Palette& resource, std::ostream& output) {
	output.put(resource.bitsPerPixel);
	output.write((char*)resource.data, resource.getDataSize());
}
}