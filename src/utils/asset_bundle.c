#include "asset_bundle.h"

#include <zf_log/zf_log.h>

void asset_bundle_free(AssetBundle* bundle) {
    string_list_free(&bundle->skel_files);
    string_list_free(&bundle->atlas_files);
    string_list_free(&bundle->png_files);
}

int asset_bundle_scan(const char* input_dir, AssetBundle* bundle) {
    *bundle = (AssetBundle){};
    return scan_assets(input_dir, &bundle->skel_files, &bundle->atlas_files, &bundle->png_files);
}

int asset_bundle_load_primary_pair(const char* input_dir, AssetBundle* bundle) {
    int result = asset_bundle_scan(input_dir, bundle);
    if (result != 0) {
        return result;
    }

    if (bundle->skel_files.count == 0 || bundle->atlas_files.count == 0) {
        ZF_LOGE("Input must contain at least one .skel and one .atlas file.");
        return -1;
    }

    AtlasPageStats page_stats;
    return validate_atlas_pages(&bundle->atlas_files, &bundle->png_files, &page_stats);
}

int asset_bundle_validate_dump_input(const char* input_dir,
                                     AssetBundle* bundle,
                                     AtlasPageStats* page_stats) {
    int result = asset_bundle_scan(input_dir, bundle);
    if (result != 0) {
        return result;
    }

    if (bundle->skel_files.count == 0 || bundle->atlas_files.count == 0 ||
        bundle->png_files.count == 0) {
        ZF_LOGE("Input must contain at least one .skel, one .atlas, and one .png file. "
                "Found %zu/%zu/%zu.",
                bundle->skel_files.count, bundle->atlas_files.count, bundle->png_files.count);
        return -1;
    }

    return validate_atlas_pages(&bundle->atlas_files, &bundle->png_files, page_stats);
}
