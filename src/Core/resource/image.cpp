#include "readerwriter.hpp"
#include <png.h>
#include <math.h>
#include <string>

// TODO review
uint16_t ShadyCore::Palette::packColor(uint32_t color, bool transparent) {
	uint16_t pcolor = !transparent;
	pcolor = (pcolor << 5) + ((color >> 19) & 0x1F);
	pcolor = (pcolor << 5) + ((color >> 11) & 0x1F);
	pcolor = (pcolor << 5) + ((color >> 3) & 0x1F);
	return pcolor;
}

uint32_t ShadyCore::Palette::unpackColor(uint16_t color) {
	uint32_t ucolor;

	//ucolor = (color << 3) & 0xF8;
	ucolor = (color >> 7) & 0xF8;
	ucolor += (ucolor >> 5) & 0x07;
	ucolor = ucolor << 8;
	//ucolor += (color >> 2) & 0xF8;
	ucolor += (color >> 2) & 0xF8;
	ucolor += (ucolor >> 5) & 0x07;
	ucolor = ucolor << 8;
	//ucolor += (color >> 7) & 0xF8;
	ucolor += (color << 3) & 0xF8;
	ucolor += (ucolor >> 5) & 0x07;

	return ucolor;
}

// TODO fix tranparency
void ShadyCore::Palette::pack() {
	if (bitsPerPixel <= 16) return;

	uint16_t* newData = (uint16_t*)ShadyAllocate(256 * 2);
	for (int i = 0; i < 256; ++i) {
		uint32_t* _data = (uint32_t*)data;
		newData[i] = packColor(_data[i]);
	}

	destroy();
	bitsPerPixel = 16;
	data = (uint8_t*)newData;
}

// TODO fix tranparency
void ShadyCore::Palette::unpack() {
	if (bitsPerPixel > 16) return;

	uint32_t* newData = (uint32_t*)ShadyAllocate(256 * 4);
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
	uint32_t width, height, paddedWidth;
	uint8_t bitsPerPixel;
	width = png_get_image_width(pngData, pngInfo);
	height = png_get_image_height(pngData, pngInfo);
	paddedWidth = ceil(width / 4.f) * 4;

	// Register transformations
	int depth = png_get_bit_depth(pngData, pngInfo);
	if (depth > 8) png_set_strip_16(pngData);
	if (depth < 8) png_set_packing(pngData);
	int colorType = png_get_color_type(pngData, pngInfo);
	if (colorType & PNG_COLOR_MASK_PALETTE) {
		bitsPerPixel = 8;
	} else {
		bitsPerPixel = colorType & PNG_COLOR_MASK_ALPHA ? 32 : 24;
		if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(pngData);
		if (!(colorType & PNG_COLOR_MASK_ALPHA)) png_set_filler(pngData, 0xff, PNG_FILLER_AFTER);
		png_set_bgr(pngData);
	} png_read_update_info(pngData, pngInfo);
	resource.initialize(bitsPerPixel, width, height, paddedWidth);
	memset(resource.getRawImage(), 0, resource.getRawSize());

	// Read Image
	//int rowSize = png_get_rowbytes(pngData, pngInfo);
	int rowSize = paddedWidth * (bitsPerPixel == 8 ? 1 : 4);
	uint8_t ** pointers = new uint8_t*[height];
	for (int i = 0; i < height; ++i) {
		pointers[i] = resource.getRawImage() + i * rowSize;
	} png_read_image(pngData, (png_bytepp)pointers);
	delete[] pointers;

	// Finish
	png_read_end(pngData, 0);
	png_destroy_read_struct(&pngData, &pngInfo, 0);
}

void readerImageCv(ShadyCore::Image& resource, std::istream& input) {
	uint32_t width, height, paddedWidth, altSize;
	uint8_t bitsPerPixel;
	input.read((char*)&bitsPerPixel, 1);
	input.read((char*)&width, 4);
	input.read((char*)&height, 4);
	input.read((char*)&paddedWidth, 4);
	input.read((char*)&altSize, 4);

	if (bitsPerPixel == 16) throw; // TODO 16bits unused
	resource.initialize(bitsPerPixel, width, height, paddedWidth, altSize);
	input.read((char*)resource.getRawImage(), resource.getRawSize());
}

void writerImagePng(ShadyCore::Image& resource, std::ostream& output) {
	// Initialize
	png_structp pngData = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, pngError, pngWarning);
	png_infop pngInfo = png_create_info_struct(pngData);
	png_set_write_fn(pngData, &output, pngWrite, pngFlush);

	// Setup Info
	int colorType
		= resource.getBitsPerPixel() == 8 ? PNG_COLOR_TYPE_PALETTE
		: resource.getBitsPerPixel() == 24 ? PNG_COLOR_TYPE_RGB
		: PNG_COLOR_TYPE_RGB_ALPHA;
	png_set_IHDR(pngData, pngInfo, resource.getWidth(), resource.getHeight(), 8, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	if (colorType == PNG_COLOR_TYPE_PALETTE) {
		png_set_PLTE(pngData, pngInfo, emptyPalette, 256);
	}

	png_write_info(pngData, pngInfo);
	if (colorType == PNG_COLOR_TYPE_RGB) png_set_filler(pngData, 0, PNG_FILLER_AFTER);
	if (colorType != PNG_COLOR_TYPE_PALETTE) png_set_bgr(pngData);

	// Write Image
	int rowSize = resource.getPaddedWidth() * (resource.getBitsPerPixel() == 8 ? 1 : 4);
	const uint8_t ** pointers = new const uint8_t*[resource.getHeight()];
	for (int i = 0; i < resource.getHeight(); ++i) {
		pointers[i] = resource.getRawImage() + i * rowSize;
	} png_write_image(pngData, (png_bytepp)pointers);
	delete[] pointers;

	// Finish
	png_write_end(pngData, pngInfo);
	png_destroy_write_struct(&pngData, &pngInfo);
}

void writerImageCv(ShadyCore::Image& resource, std::ostream& output) {
	output.write((char*)&resource.getBitsPerPixel(), 1);
	output.write((char*)&resource.getWidth(), 4);
	output.write((char*)&resource.getHeight(), 4);
	output.write((char*)&resource.getPaddedWidth(), 4);
	output.write((char*)&resource.getAltSize(), 4);

	if (resource.getBitsPerPixel() == 16) throw; // TODO 16bits unused
	output.write((char*)resource.getRawImage(), resource.getRawSize());
}

void readerPaletteAct(ShadyCore::Palette& resource, std::istream& input) {
	resource.initialize(16);
	uint16_t* data = (uint16_t*)resource.getData();
	for (int i = 0; i < 256; ++i) {
		uint32_t color = 0;
		input.read((char*)&color, 3);
		data[i] = ShadyCore::Palette::packColor(color, i == 0);
	}
}

void readerPalette(ShadyCore::Palette& resource, std::istream& input) {
	uint8_t bitsPerPixel;
	input.read((char*)&bitsPerPixel, 1);
	resource.initialize(bitsPerPixel);
	input.read((char*)resource.getData(), resource.getSize());
}

void writerPaletteAct(ShadyCore::Palette& resource, std::ostream& output) {
	if (resource.getBitsPerPixel() <= 16) {
		for (int i = 0; i < 256; ++i) {
			uint16_t* data = (uint16_t*)resource.getData();
			uint32_t color = resource.unpackColor(data[i]);
			output.write((char*)&color, 3);
		}
	} else {
		for (int i = 0; i < 256; ++i) {
			uint32_t* data = (uint32_t*)resource.getData();
			output.write((char*)&data[i], 3);
		}
	}
}

void writerPalette(ShadyCore::Palette& resource, std::ostream& output) {
	output.put(resource.getBitsPerPixel());
	output.write((char*)resource.getData(), resource.getSize());
}
}