#include <gtest/gtest.h>
#include <melon/core/handle.h>
#include <melon/core/error.h>

class PoolStressTest : public ::testing::TestWithParam<size_t>
{
public:
};

INSTANTIATE_TEST_CASE_P(CapacityTest, PoolStressTest, ::testing::Values((size_t) 0, (size_t) 1024));

TEST_P(PoolStressTest, past_max_test)
{
    melon_handle_pool pool;
    melon_create_handle_pool(&pool, GetParam(), melon_default_cb_allocator(), false);   

    for (size_t i = 0; i < GetParam(); i++)
    {
        melon_handle handle = melon_pool_create_handle(&pool);
        EXPECT_EQ(i, melon_handle_index(handle));
    }

    melon_handle handle = melon_pool_create_handle(&pool);
    EXPECT_EQ(MELON_INVALID_HANDLE, handle);

    melon_delete_handle_pool(&pool);
}

TEST_P(PoolStressTest, past_max_test_growable)
{
    const size_t init_capacity = 1;
    melon_handle_pool pool;
    melon_create_handle_pool(&pool, init_capacity, melon_default_cb_allocator(), true);   

    for (size_t i = 0; i < GetParam(); i++)
    {
        melon_handle handle = melon_pool_create_handle(&pool);
        EXPECT_EQ(i, melon_handle_index(handle));
    }

    melon_delete_handle_pool(&pool);
}

typedef struct
{
    const char* string;
    int         value_i;
    float       value_f;
} test_type;

MELON_HANDLE_MAP_TYPEDEF(test_type);

TEST_P(PoolStressTest, map_past_max_test_growable)
{
    const size_t      init_capacity = 1;
    const size_t iterations = GetParam();
    melon_map_test_type map;
    melon_create_map(&map, init_capacity, melon_default_cb_allocator(), true);

    melon_handle* handles = (melon_handle*) malloc(iterations * sizeof(melon_handle));
    test_type* test_types = (test_type*) malloc(iterations * sizeof(test_type));
    for (size_t i = 0; i < iterations; i++)
    {
        static const char* test_string = "test";
        test_types[i].string = test_string;
        test_types[i].value_i = i;
        test_types[i].value_f = (float) i;

        handles[i] = melon_map_push(&map, test_types + i);
    }

    for (size_t i = 0; i < iterations; i++)
    {
        const test_type* test_type = melon_map_get(&map, handles[i]);

        //EXPECT_EQ(0, strcmp(test_types[i].string, test_type->string));
        EXPECT_EQ(test_types[i].value_i, test_type->value_i);
        EXPECT_EQ(test_types[i].value_f, test_type->value_f);
    }

    free(handles);
    free(test_types);
    melon_delete_map(&map);
}

TEST_P(PoolStressTest, double_free)
{
    melon_handle_pool pool;
    melon_create_handle_pool(&pool, GetParam(), melon_default_cb_allocator(), false);

    melon_handle* handles = (melon_handle*) malloc(GetParam() * sizeof(melon_handle));
    for (size_t i = 0; i < GetParam(); i++)
    {
        handles[i] = melon_pool_create_handle(&pool);
        EXPECT_EQ(true, melon_pool_delete_handle(&pool, handles[i]));
    }

    for (size_t i = 0; i < GetParam(); i++)
    {
        EXPECT_EQ(false, melon_handle_is_valid(&pool, handles[i]));
    }

    free(handles);
    melon_delete_handle_pool(&pool);
}

TEST(LimitTests, generation_test)
{
    melon_handle_pool pool;
    melon_create_handle_pool(&pool, 1, melon_default_cb_allocator(), false);

    pool.handle_entries[0].handle = MELON_HANDLE_GENERATION_MASK - ((size_t) 1 << MELON_HANDLE_INDEX_BITS);

    EXPECT_EQ(true, melon_pool_delete_handle(&pool, melon_pool_create_handle(&pool)));

    melon_handle handle = melon_pool_create_handle(&pool);
    EXPECT_EQ(MELON_HANDLE_GENERATION_MAX, handle >> MELON_HANDLE_INDEX_BITS);
    EXPECT_EQ(MELON_INVALID_HANDLE, handle);

    melon_delete_handle_pool(&pool);
}