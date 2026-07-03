#include "image_io.h"

#include <stdio.h>
#include <stdlib.h>

#include <spng.h>

#include "file.h"

static int read_file(const char* filename, unsigned char** data, size_t* size) {
    FILE* file = file_open(filename, "rb");
    if (file == NULL) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return -1;
    }
    rewind(file);

    unsigned char* bytes = malloc((size_t)length);
    if (bytes == NULL && length > 0) {
        fclose(file);
        return -1;
    }
    size_t read = fread(bytes, 1, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) {
        free(bytes);
        return -1;
    }

    *data = bytes;
    *size = (size_t)length;
    return 0;
}

PngEncodeOptions png_encode_options_for(PngCompressionPreset preset) {
    switch (preset) {
        case PNG_COMPRESSION_FAST:
            return (PngEncodeOptions){.compression_level = 1,
                                      .filter_choice = SPNG_FILTER_CHOICE_NONE};
        case PNG_COMPRESSION_SMALL:
            return (PngEncodeOptions){.compression_level = 9,
                                      .filter_choice = SPNG_FILTER_CHOICE_ALL};
        case PNG_COMPRESSION_BALANCED:
        default:
            return (PngEncodeOptions){.compression_level = 6,
                                      .filter_choice = SPNG_FILTER_CHOICE_NONE};
    }
}

int image_decode_png32_file(unsigned char** out,
                            unsigned* width,
                            unsigned* height,
                            const char* filename) {
    unsigned char* file_data = NULL;
    size_t file_size = 0;
    if (read_file(filename, &file_data, &file_size) != 0) {
        return -1;
    }

    spng_ctx* ctx = spng_ctx_new(0);
    if (ctx == NULL) {
        free(file_data);
        return -1;
    }

    int error = spng_set_png_buffer(ctx, file_data, file_size);
    struct spng_ihdr ihdr = {};
    if (error == 0) {
        error = spng_get_ihdr(ctx, &ihdr);
    }

    size_t output_size = 0;
    if (error == 0) {
        error = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &output_size);
    }

    unsigned char* pixels = NULL;
    if (error == 0) {
        pixels = malloc(output_size);
        if (pixels == NULL) {
            error = SPNG_EMEM;
        }
    }
    if (error == 0) {
        error = spng_decode_image(ctx, pixels, output_size, SPNG_FMT_RGBA8, 0);
    }

    spng_ctx_free(ctx);
    free(file_data);

    if (error != 0) {
        free(pixels);
        return error;
    }

    *out = pixels;
    *width = ihdr.width;
    *height = ihdr.height;
    return 0;
}

int image_encode_png32_file(const char* filename,
                            const unsigned char* image,
                            unsigned width,
                            unsigned height,
                            const PngEncodeOptions* options) {
    spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    if (ctx == NULL) {
        return -1;
    }

    struct spng_ihdr ihdr = {};
    ihdr.width = width;
    ihdr.height = height;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    ihdr.bit_depth = 8;

    int error = spng_set_ihdr(ctx, &ihdr);
    if (error == 0) {
        error = spng_set_option(ctx, SPNG_ENCODE_TO_BUFFER, 1);
    }
    if (error == 0 && options != NULL) {
        error = spng_set_option(ctx, SPNG_IMG_COMPRESSION_LEVEL, options->compression_level);
    }
    if (error == 0 && options != NULL) {
        error = spng_set_option(ctx, SPNG_FILTER_CHOICE, options->filter_choice);
    }
    if (error == 0) {
        error = spng_encode_image(ctx, image, (size_t)width * (size_t)height * 4, SPNG_FMT_PNG,
                                  SPNG_ENCODE_FINALIZE);
    }

    void* png = NULL;
    size_t png_size = 0;
    if (error == 0) {
        png = spng_get_png_buffer(ctx, &png_size, &error);
    }

    if (error == 0) {
        FILE* file = file_open(filename, "wb");
        if (file == NULL) {
            error = -1;
        } else {
            size_t written = fwrite(png, 1, png_size, file);
            fclose(file);
            if (written != png_size) {
                error = -1;
            }
        }
    }

    free(png);
    spng_ctx_free(ctx);
    return error;
}
