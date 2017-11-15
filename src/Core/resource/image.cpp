#include "readerwriter.hpp"
#include <png.h>
#include <math.h>

ShadyCore::Image::~Image() { if (raw) delete[] raw; }

void ShadyCore::Image::initialize(uint32_t w, uint32_t h, uint32_t p, uint8_t b) {
	width = w; height = h; paddedWidth = p; bitsPerPixel = b;

	if (raw) delete[] raw;
	raw = new uint8_t[width * height * (bitsPerPixel == 8 ? 1 : 4)];
}

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

void ShadyCore::ResourceDReader::accept(Image& resource) {
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
	resource.initialize(width, height, paddedWidth, bitsPerPixel);

	// Read Image
	int rowSize = png_get_rowbytes(pngData, pngInfo);
	uint8_t ** pointers = new uint8_t*[height];
	for (int i = 0; i < height; ++i) {
		pointers[i] = resource.getRawImage() + i * rowSize;
	} png_read_image(pngData, (png_bytepp)pointers);
	delete[] pointers;

	// Finish
	png_read_end(pngData, 0);
	png_destroy_read_struct(&pngData, &pngInfo, 0);
}

void ShadyCore::ResourceEReader::accept(Image& resource) {
	uint32_t width, height, paddedWidth;
	uint8_t bitsPerPixel;
	input.read((char*)&bitsPerPixel, 1);
	input.read((char*)&width, 4);
	input.read((char*)&height, 4);
	input.read((char*)&paddedWidth, 4);
	if (bitsPerPixel == 16) throw; // TODO 16bits unused
	resource.initialize(width, height, paddedWidth, bitsPerPixel);

	uint32_t lineSize = width * (bitsPerPixel == 8 ? 1 : 4);
	int padSize = (paddedWidth - width) * (bitsPerPixel == 8 ? 1 : 4);

	input.ignore(4);

	uint8_t* data = resource.getRawImage();
	for (uint32_t i = 0; i < height; i++, data += lineSize) {
		input.read((char*)data, lineSize);
		input.ignore(padSize);
	}
}

void ShadyCore::ResourceDWriter::accept(Image& resource) {
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
	int rowSize = resource.getWidth() * (resource.getBitsPerPixel() == 8 ? 1 : 4);
	const uint8_t ** pointers = new const uint8_t*[resource.getHeight()];
	for (int i = 0; i < resource.getHeight(); ++i) {
		pointers[i] = resource.getRawImage() + i * rowSize;
	} png_write_image(pngData, (png_bytepp)pointers);
	delete[] pointers;

	// Finish
	png_write_end(pngData, pngInfo);
	png_destroy_write_struct(&pngData, &pngInfo);
}

void ShadyCore::ResourceEWriter::accept(Image& resource) {
	output.write((char*)&resource.getBitsPerPixel(), 1);
	output.write((char*)&resource.getWidth(), 4);
	output.write((char*)&resource.getHeight(), 4);
	output.write((char*)&resource.getPaddedWidth(), 4);
	if (resource.getBitsPerPixel() == 16) throw; // TODO 16bits unused

	uint32_t lineSize = resource.getWidth() * (resource.getBitsPerPixel() == 8 ? 1 : 4);
	int padSize = (resource.getPaddedWidth() - resource.getWidth()) * (resource.getBitsPerPixel() == 8 ? 1 : 4);

	output.write("\0\0\0\0", 4);
	const uint8_t* data = resource.getRawImage();
	for (uint32_t i = 0; i < resource.getHeight(); i++, data += lineSize) {
		output.write((const char*)data, lineSize);
		for (int j = 0; j < padSize; ++j) output.put(0);
	}
}

void ShadyCore::ResourceDReader::accept(Palette& resource) {
	input.read((char*)resource.getData(), 256 * 3);
}

void ShadyCore::ResourceEReader::accept(Palette& resource) {
	uint16_t pcolor;

	input.ignore(1);

	for (int i = 0; i < 256; ++i) {
		input.read((char*)&pcolor, 2);
		resource.setColor(i, Palette::unpackColor(pcolor));
	}
}

void ShadyCore::ResourceDWriter::accept(Palette& resource) {
	output.write((char*)resource.getData(), 256 * 3);
}

void ShadyCore::ResourceEWriter::accept(Palette& resource) {
	uint16_t pcolor;

	output.put(0x10);

	for (int i = 0; i < 256; ++i) {
		pcolor = Palette::packColor(resource.getColor(i), i == 0);
		output.write((char*)&pcolor, 2);
	}
}
