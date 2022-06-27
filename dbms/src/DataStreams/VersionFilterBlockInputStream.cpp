// Copyright 2022 PingCAP, Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Columns/ColumnsCommon.h>
#include <Columns/ColumnsNumber.h>
#include <DataStreams/VersionFilterBlockInputStream.h>
#include <DataStreams/dedupUtils.h>
#include <common/mem_utils.h>
#include <common/mutliversioned_vectorization.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
}

__attribute__((always_inline)) inline Block VersionFilterBlockInputStream::readInlined()
{
    while (true)
    {
        Block block = input->read();
        if (!block)
            return block;

        const ColumnWithTypeAndName & version_column = block.getByPosition(version_column_index);
        const auto * column = static_cast<const ColumnUInt64 *>(version_column.column.get());

        size_t rows = block.rows();

        const UInt64 * __restrict data_start = column->getData().data();
        const UInt64 * __restrict data_end = data_start + rows;
        const UInt64 * __restrict filter_start = nullptr;

        {
            using mask_t = uint64_t;
            static constexpr size_t mask_size = 8 * sizeof(mask_t);
            size_t i = 0;
            for (; i + mask_size < rows; i += mask_size)
            {
                mask_t mask = 0;
                const auto * current = data_start + i;
#pragma clang loop vectorize_width(scalable) vectorize_predicate(enable)
                for (size_t j = 0; j < mask_size; ++j)
                {
                    mask |= (current[j] > filter_greater_version) ? (static_cast<mask_t>(1u) << j) : 0;
                }
                if (mask)
                {
                    filter_start = current + __builtin_ctzll(mask);
                    break;
                }
            }

            if (filter_start == nullptr)
            {
                for (; i < rows; ++i)
                {
                    if (data_start[i] > filter_greater_version)
                    {
                        filter_start = data_start + i;
                        break;
                    }
                }
            }
        }

        if (filter_start == nullptr)
            return block;

        IColumn::Filter col_filter(rows, 1);

        {
            UInt8 * filter_pos = col_filter.data() + (filter_start - data_start);
            const UInt64 * data_pos = filter_start;
            for (; data_pos != data_end; ++data_pos, ++filter_pos)
                filter_pos[0] = data_pos[0] <= filter_greater_version;
        }

        if (filter_start == data_start && mem_utils::memoryIsZero(col_filter.data(), rows))
            continue;

        return filterBlock(block, col_filter);
    }
}

TIFLASH_MULTIVERSIONED_VECTORIZATION(
    Block,
    VersionFilterBlockInputStreamRead,
    (VersionFilterBlockInputStream & self),
    (self),
    {
        return self.readInlined();
    })

Block VersionFilterBlockInputStream::readImpl()
{
    return VersionFilterBlockInputStreamReadTiFlashMultiVersion::invoke(*this);
}

} // namespace DB