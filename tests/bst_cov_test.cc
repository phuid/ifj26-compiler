#include "gtest/gtest.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <utility>
#include <vector>

extern "C" {
#include "../src/bst.h"
}

namespace {

/*
 * AI-generated test block — counting BST fixture.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Provide a fixture for black-box tests of bst.h: an entry type with a key
 *   and a payload the comparator ignores, a comparator that counts its calls,
 *   copy/destroy callbacks that count live entries (checked to be zero after
 *   bstDestroy) and the AVL height bound for n nodes.
 */
struct Entry {
  int key;
  int payload;
};

long compareCalls = 0;
long liveEntries = 0;

int compareEntries(const void *a, const void *b) {
  ++compareCalls;
  const int ka = static_cast<const Entry *>(a)->key;
  const int kb = static_cast<const Entry *>(b)->key;
  return (ka > kb) - (ka < kb);
}

void *copyEntry(const void *value) {
  ++liveEntries;
  return new Entry(*static_cast<const Entry *>(value));
}

void destroyEntry(void *value) {
  --liveEntries;
  delete static_cast<Entry *>(value);
}

// Upper bound on the height of an AVL tree with n nodes
// (Adelson-Velsky & Landis): h < 1.4405 * log2(n + 2) - 0.3277.
int avlHeightBound(size_t n) {
  return static_cast<int>(
      std::floor(1.4405 * std::log2(static_cast<double>(n) + 2.0) - 0.3277));
}

class BSTCovTest : public ::testing::Test {
protected:
  void SetUp() override {
    compareCalls = 0;
    liveEntries = 0;
    tree = bstCreate(compareEntries, copyEntry, destroyEntry);
    ASSERT_NE(tree, nullptr);
  }
  void TearDown() override {
    bstDestroy(&tree);
    EXPECT_EQ(tree, nullptr);
    EXPECT_EQ(liveEntries, 0) << "tree leaked or double-freed entries";
  }

  // Number of comparator calls one bstSearch for key needs.
  long searchCost(int key, bool *found) {
    const Entry probe{key, 0};
    compareCalls = 0;
    *found = bstSearch(tree, &probe) != nullptr;
    return compareCalls;
  }

  // Payload stored for key, or -1 when bstSearch does not find it.
  int payloadOf(int key) const {
    const Entry probe{key, -1};
    const Entry *found = static_cast<const Entry *>(bstSearch(tree, &probe));
    return found == nullptr ? -1 : found->payload;
  }

  BST *tree = nullptr;
};

} // namespace

/*
 * AI-generated test block — logarithmic search cost (black-box AVL height).
 * Model: Claude (Claude Code).
 * Prompt:
 *   bst.c promises a height-balanced tree; check through the public API only
 *   (by counting comparator calls of bstSearch) that the worst lookup stays
 *   within kCallsPerLevel = 2 times the AVL height bound for n keys, for
 *   ascending, descending and portably shuffled insertion orders and after
 *   removing every other key; one assertion per phase.
 */
TEST_F(BSTCovTest, SearchComparisonCountStaysLogarithmic) {
  constexpr int kCount = 1000;
  // A three-way search may legitimately call the comparator up to twice per
  // visited level, so only that much is assumed about bstSearch internals.
  constexpr int kCallsPerLevel = 2;
  std::vector<int> ascending(kCount);
  for (int i = 0; i < kCount; ++i) {
    ascending[i] = i;
  }
  std::vector<int> descending(ascending.rbegin(), ascending.rend());
  // Portable Fisher-Yates: raw mt19937 output is fully specified by the
  // standard, unlike std::shuffle / std::uniform_int_distribution.
  std::vector<int> shuffled(ascending);
  std::mt19937 rng(7U);
  for (int i = kCount - 1; i > 0; --i) {
    std::swap(shuffled[i], shuffled[rng() % static_cast<unsigned>(i + 1)]);
  }

  // Worst comparator-call count over all lookups; also checks found/not found.
  auto worstSearchCost = [this](int lo, int hi, auto shouldExist) {
    long worst = 0;
    for (int key = lo; key <= hi; ++key) {
      bool found = false;
      worst = std::max(worst, searchCost(key, &found));
      EXPECT_EQ(found, shouldExist(key)) << "key " << key;
    }
    return worst;
  };

  for (const std::vector<int> *order : {&ascending, &descending, &shuffled}) {
    SCOPED_TRACE(order == &ascending    ? "ascending"
                 : order == &descending ? "descending"
                                        : "shuffled");
    for (int key : *order) {
      const Entry entry{key, key};
      bstInsert(tree, &entry);
    }
    EXPECT_LE(
        worstSearchCost(-1, kCount, [](int k) { return k >= 0 && k < kCount; }),
        kCallsPerLevel * avlHeightBound(kCount));

    for (int key = 0; key < kCount; key += 2) {
      const Entry entry{key, 0};
      bstRemove(tree, &entry);
    }
    EXPECT_LE(worstSearchCost(
                  -1, kCount,
                  [](int k) { return k >= 0 && k < kCount && k % 2 == 1; }),
              kCallsPerLevel * avlHeightBound(kCount / 2));
    bstDestroy(&tree);
    EXPECT_EQ(liveEntries, 0);
    tree = bstCreate(compareEntries, copyEntry, destroyEntry);
    ASSERT_NE(tree, nullptr);
  }
}

/*
 * AI-generated test block — duplicate insert at inner nodes.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Insert keys 1..7, then insert keys 4 and 2 again with a different
 *   payload; bstSearch must return the new payload for them, the other keys
 *   keep their payloads and the tree still holds exactly seven entries.
 */
TEST_F(BSTCovTest, DuplicateInsertReplacesPayloadOfInnerNodes) {
  for (int key = 1; key <= 7; ++key) {
    const Entry entry{key, key * 10};
    bstInsert(tree, &entry);
  }
  ASSERT_EQ(liveEntries, 7);

  // AVL insertion of ascending 1..7 yields the perfect tree, so 4 is the
  // root and 2 an inner node with two children.
  const Entry newRoot{4, 400};
  const Entry newInner{2, 200};
  bstInsert(tree, &newRoot);
  bstInsert(tree, &newInner);

  EXPECT_EQ(liveEntries, 7);
  EXPECT_EQ(payloadOf(4), 400);
  EXPECT_EQ(payloadOf(2), 200);
  for (int key : {1, 3, 5, 6, 7}) {
    EXPECT_EQ(payloadOf(key), key * 10) << "key " << key;
  }
  EXPECT_EQ(payloadOf(0), -1);
  EXPECT_EQ(payloadOf(8), -1);
}

/*
 * AI-generated test block — duplicate insert replaces payload at any depth.
 * Model: Claude (Claude Code).
 * Prompt:
 *   A duplicate bstInsert must replace the stored value of its key wherever
 *   the node sits (inner nodes, nodes with two children); check bstSearch
 *   payloads against std::map over random inserts, duplicate updates and
 *   removes with a fixed mt19937 seed.
 */
TEST_F(BSTCovTest, PayloadFollowsKeyThroughRandomOperations) {
  constexpr int kKeyRange = 300;
  std::mt19937 rng(20261117U);
  std::map<int, int> reference;
  for (int step = 0; step < 5000 && !HasFailure(); ++step) {
    const int key = static_cast<int>(rng() % kKeyRange);
    const Entry entry{key, static_cast<int>(rng() % 100000U)};
    if (rng() % 3U != 0U) {
      bstInsert(tree, &entry);
      reference[key] = entry.payload; // duplicate key replaces the payload
    } else {
      bstRemove(tree, &entry);
      reference.erase(key);
    }
    if (step % 50 == 0 || step == 4999) {
      for (int k = 0; k < kKeyRange; ++k) {
        const Entry probe{k, -1};
        const Entry *found =
            static_cast<const Entry *>(bstSearch(tree, &probe));
        const auto it = reference.find(k);
        if (it == reference.end()) {
          EXPECT_EQ(found, nullptr) << "step " << step << ", key " << k;
        } else if (found == nullptr) {
          ADD_FAILURE() << "step " << step << ", missing key " << k;
        } else {
          EXPECT_EQ(found->key, k) << "step " << step;
          EXPECT_EQ(found->payload, it->second)
              << "step " << step << ", key " << k;
        }
      }
      EXPECT_EQ(liveEntries, static_cast<long>(reference.size()))
          << "step " << step;
    }
  }
}
