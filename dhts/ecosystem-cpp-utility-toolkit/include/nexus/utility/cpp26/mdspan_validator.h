#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

namespace nexus::utility::cpp26 {

/// Validate C++26 mdspan usage (submdspan, custom layouts, accessors)
class MdspanValidator {
public:
    struct ExtentsInfo {
        std::vector<size_t> extents;
        size_t rank;
        size_t totalSize;
        bool isDynamic;
    };

    struct SubmdspanResult {
        bool valid = true;
        std::string error;
        std::vector<size_t> sliceOffsets;
        std::vector<size_t> resultExtents;
    };

    ExtentsInfo describeExtents(const std::vector<size_t>& ext) {
        ExtentsInfo info;
        info.extents = ext;
        info.rank = ext.size();
        info.totalSize = 1;
        info.isDynamic = false;
        for (auto e : ext) {
            if (e == std::dynamic_extent) info.isDynamic = true;
            else info.totalSize *= e;
        }
        return info;
    }

    SubmdspanResult validateSlice(const std::vector<size_t>& extents,
                                   const std::vector<std::pair<size_t, size_t>>& slices) {
        SubmdspanResult r;
        if (slices.size() > extents.size()) {
            r.valid = false;
            r.error = "Too many slices: " + std::to_string(slices.size()) +
                      " > rank " + std::to_string(extents.size());
            return r;
        }
        for (size_t i = 0; i < slices.size(); i++) {
            if (slices[i].first >= extents[i]) {
                r.valid = false;
                r.error = "Slice offset out of bounds at dimension " + std::to_string(i);
                return r;
            }
            r.sliceOffsets.push_back(slices[i].first);
            size_t resultSize = slices[i].second - slices[i].first;
            if (resultSize > 0) r.resultExtents.push_back(resultSize);
        }
        return r;
    }

    static bool isRowMajor(size_t stride1, size_t stride2, size_t dim1, size_t dim2) {
        return stride1 >= dim2 && stride2 == 1;
    }

    static bool isColumnMajor(size_t stride1, size_t stride2, size_t dim1, size_t dim2) {
        return stride1 == 1 && stride2 >= dim1;
    }
};
} // namespace nexus::utility::cpp26
