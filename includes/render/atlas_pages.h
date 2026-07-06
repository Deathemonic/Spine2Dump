#ifndef SPINE2DUMP_ATLAS_PAGES_H
#define SPINE2DUMP_ATLAS_PAGES_H

typedef struct LoadedPage {
    char* name;
    unsigned char* pixels;
    int width;
    int height;
    int channels;
} LoadedPage;

typedef struct CpuAtlasPages {
    LoadedPage* items;
    int count;
} CpuAtlasPages;

#endif
