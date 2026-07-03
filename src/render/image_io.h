#ifndef SPINE2DUMP_IMAGE_IO_H
#define SPINE2DUMP_IMAGE_IO_H

int image_decode_png32_file(unsigned char** out,
                            unsigned* width,
                            unsigned* height,
                            const char* filename);
int image_encode_png32_file(const char* filename,
                            const unsigned char* image,
                            unsigned width,
                            unsigned height);

#endif
