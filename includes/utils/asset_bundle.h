#ifndef SPINE2DUMP_ASSET_BUNDLE_H
#define SPINE2DUMP_ASSET_BUNDLE_H

#include "assets.h"

typedef struct {
    StringList skel_files;
    StringList atlas_files;
    StringList png_files;
} AssetBundle;

void asset_bundle_free(AssetBundle* bundle);
int asset_bundle_scan(const char* input_dir, AssetBundle* bundle);
int asset_bundle_load_primary_pair(const char* input_dir, AssetBundle* bundle);
int asset_bundle_validate_dump_input(const char* input_dir,
                                     AssetBundle* bundle,
                                     AtlasPageStats* page_stats);

#endif
