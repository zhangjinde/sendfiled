/*
  Copyright (c) 2015, Francois Kritzinger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <gtest/gtest.h>

#include "impl/server_xfer_table.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wglobal-constructors"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wpadded"

namespace {

std::size_t hash(void* p)
{
    return *static_cast<size_t*>(p);
}

template<size_t NELEMS>
struct XferTableFix : public ::testing::Test {
    XferTableFix() {
        if (!xfer_table_construct(&tbl, hash, NELEMS))
            throw std::runtime_error("Couldn't construct table");
    }

    ~XferTableFix() {
        xfer_table_destruct(&tbl, nullptr);
    }

    xfer_table tbl;
};

using XferTableFix100 = XferTableFix<100>;

} // namespace

TEST(XferTable, construct)
{
    constexpr size_t maxfds {100};

    struct xfer_table tbl;

    EXPECT_TRUE(xfer_table_construct(&tbl, hash, maxfds));

    EXPECT_EQ(128, tbl.capacity);

    for (size_t i = 0; i < 1000; i++)
        EXPECT_EQ(nullptr, xfer_table_find(&tbl, i));

    xfer_table_destruct(&tbl, nullptr);
}

TEST_F(XferTableFix100, insert_retrieve_erase_single_element)
{
    size_t e {111};

    EXPECT_TRUE(xfer_table_insert(&tbl, &e));
    EXPECT_EQ(&e, xfer_table_find(&tbl, e));

    xfer_table_erase(&tbl, e);

    EXPECT_EQ(nullptr, xfer_table_find(&tbl, e));
}

/**
 * Fills table with integers from 0-max; each bucket should be equally full
 * (i.e., to capacity)
 */
TEST_F(XferTableFix100, fill_table_to_capacity)
{
    const size_t nelems {tbl.capacity};

    std::vector<size_t> elems(nelems);

    for (size_t i = 0; i < nelems; i++) {
        elems[i] = i;
        EXPECT_TRUE(xfer_table_insert(&tbl, &elems[i]));
    }

    // Table should be full and therefore refuse new elements
    size_t overflow {nelems};
    EXPECT_FALSE(xfer_table_insert(&tbl, &overflow));

    // Table should be filled should all be filled
    for (size_t i = 0; i < tbl.capacity; i++)
        ASSERT_NE(nullptr, tbl.elems[i]);

    // Check that all inserted hashes can be retrieved
    for (size_t i = 0; i < nelems; i++) {
        auto e = static_cast<size_t*>(xfer_table_find(&tbl, i));
        ASSERT_NE(nullptr, e);
        ASSERT_EQ(i, *e);
    }
}

TEST_F(XferTableFix100, erase)
{
    const size_t nelems {tbl.capacity};

    std::vector<size_t> elems(nelems);

    for (size_t i = 0; i < nelems; i++) {
        elems[i] = i;
        EXPECT_TRUE(xfer_table_insert(&tbl, &elems[i]));
    }

    // Erase elements in random order and check that all remaining elements are
    // still retrievable.
    std::vector<size_t> shuffled_elems {elems};
    std::random_shuffle(shuffled_elems.begin(), shuffled_elems.end());

    while (!shuffled_elems.empty()) {
        xfer_table_erase(&tbl, shuffled_elems.back());

        ASSERT_EQ(nullptr, xfer_table_find(&tbl, shuffled_elems.back()));

        shuffled_elems.pop_back();

        // Check that all remaining hashes can be retrieved
        for (auto elem : shuffled_elems) {
            auto e = static_cast<size_t*>(xfer_table_find(&tbl, elem));
            ASSERT_NE(nullptr, e);
            ASSERT_EQ(elem, *e);
        }
    }
}

#pragma GCC diagnostic pop
