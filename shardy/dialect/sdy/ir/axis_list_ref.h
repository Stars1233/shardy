/* Copyright 2024 The Shardy Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "shardy/dialect/sdy/ir/dialect.h"  // IWYU pragma: keep

namespace mlir {
namespace sdy {

// A reference to a list of AxisRefs, that provides API for trimming the list
// such that the last AxisRef can be replaced, while keeping it a reference.
//
// This is useful for getting a prefix of the list of axes, where the last axis
// is also a prefix sub-axis of an axis in the original list.
class AxisListRef {
 public:
  // Assumes that input `axisRefs` is non-empty.
  AxisListRef(ArrayRef<AxisRefAttr> axisRefs)
      : axisRefs(axisRefs.drop_back()), tailAxisRef(axisRefs.back()) {}

  AxisListRef() = default;
  AxisListRef(bool isTombstone) : isTombstone(isTombstone) {}

  // TODO(enver): Define an iterator that iterates on the concatenation of
  // axisRefs and tail, and use it for the methods below.

  // Checks if the axes is empty.
  bool empty() const {
    // If `tailAxisRef` is empty, then `axisRefs` is empty as well. Hence, it is
    // sufficient to check if `tailAxisRef` empty.
    return !tailAxisRef;
  }

  int64_t size() const { return empty() ? 0 : axisRefs.size() + 1; }

  bool operator<(const AxisListRef& rhs) const;

  bool operator==(const AxisListRef& rhs) const {
    return axisRefs == rhs.axisRefs && tailAxisRef == rhs.tailAxisRef;
  }

  SmallVector<AxisRefAttr> toVector() const;

  std::pair<ArrayRef<AxisRefAttr>, AxisRefAttr> toPair() const {
    return std::make_pair(axisRefs, tailAxisRef);
  }

  // Checks if this axes is a strict prefix of the axes of `rhs`.
  bool strictPrefixOf(const AxisListRef& rhs) const;

  // Returns the product of the sharding sizes of all individual axes
  int64_t getShardingSize(MeshAttr mesh) const;

  // Returns the product of the sharding sizes of all individual axes excluding
  // the `prefix`.
  //
  // Assumes `prefix` is a prefix of this `AxisListRef`.
  int64_t getExpandedShardingSize(MeshAttr mesh,
                                  const AxisListRef& prefix) const {
    return getShardingSize(mesh) / prefix.getShardingSize(mesh);
  }
  // Truncates `this` to its largest prefix so that it does not overlap with
  // `rhs`. Returns true if `this` has been truncated, and false otherwise,
  // which happens if `this` did not overlap with `rhs` in the first place.
  bool truncateWithoutOverlap(const AxisListRef& rhs);

 private:
  // Returns prefix of input `axisRef` that does not overlap with this axes.
  // TODO(enver): Move this method to utilities.
  // TODO(enver): Instead make this a method of AxisRefAttr, after moving
  // AxesWithTail to a general data structure in Shardy.
  // TODO(enver): Reuse getPrefixOfInputWithout method on
  // shardy/dialect/sdy/transforms/propagation/basic_factor_propagation.cc,
  // instead, after an iterater is added.
  std::optional<AxisRefAttr> getPrefixOfInputWithoutOverlap(
      AxisRefAttr axisRef) const;

  // Trims axes to have the first `newSizeExcludingNewTail` axes and, in case
  // non-empty, `newTailAxisRef` as an additional final axis.

  // As a result, `newSizeExcludingNewTail` is the new size of AxisListRef
  // excluding `newTailAxisRef`. That is, if `newTailAxisRef` is non-empty then
  // the new size of AxisListRef equals to `newSizeExcludingNewTail`+1,
  // otherwise it equals to `newSizeExcludingNewTail`.
  //
  // Assumes that:
  //  1. `this` AxisListRef is non-empty, and
  //  2. `newSize` is strictly smaller than size().
  //  3. Input `newTailAxisRef` is a prefix of the (`newSize`+1)st axis.
  void trim(int64_t newSizeExcludingNewTail,
            std::optional<AxisRefAttr> newTailAxisRef);
  // Clears this AxisListRef.
  void clear();

  // The axes that this FactorAxesPair holds is defined by `axisRefs` and
  // `tailAxisRef` together as the concatantion of the two. If `tailAxisRef` is
  // empty, then `axisRefs` is empty as well.
  ArrayRef<AxisRefAttr> axisRefs;
  AxisRefAttr tailAxisRef;
  // TODO(enver): Use ArrayRef::getTombstoneKey or AxisRefAttr::getTombstoneKey,
  // either for `axisRefs` or `tailAxisRef` respectively, instead.
  bool isTombstone = false;
};

struct AxisListRefInfo : public llvm::DenseMapInfo<AxisListRef> {
  static unsigned getHashValue(const AxisListRef& m) {
    return llvm::hash_value(m.toPair());
  }
  static bool isEqual(const AxisListRef& lhs, const AxisListRef& rhs) {
    return lhs == rhs;
  }

  static inline AxisListRef getEmptyKey() { return AxisListRef(); }

  static inline AxisListRef getTombstoneKey() {
    return AxisListRef(/*isTombstone=*/true);
  }
};

}  // namespace sdy
}  // namespace mlir