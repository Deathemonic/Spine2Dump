#ifndef SPINE2DUMP_IMAGE_IO_H
#define SPINE2DUMP_IMAGE_IO_H

typedef struct PngEncodeOptions {
    int compression_level;
    int filter_choice;
} PngEncodeOptions;

typedef enum PngCompressionPreset {
    PNG_COMPRESSION_FAST = 0,
    PNG_COMPRESSION_BALANCED = 1,
    PNG_COMPRESSION_SMALL = 2
} PngCompressionPreset;

PngEncodeOptions png_encode_options_for(PngCompressionPreset preset);

int image_decode_png32_file(unsigned char** out,
                            unsigned* width,
                            unsigned* height,
                            const char* filename);
int image_encode_png32_file(const char* filename,
                            const unsigned char* image,
                            unsigned width,
                            unsigned height,
                            const PngEncodeOptions* options);

#endif
