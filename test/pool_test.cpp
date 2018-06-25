#include <melon/core.hpp>
#include <melon/error.hpp>

int main()
{
    const size_t test_capacity = 12;
    melon::IndexPool pool;
    melon::create_index_pool(&pool, test_capacity, melon::default_cb_allocator());

    MELON_ASSERT(pool.free_indices != NULL);
    MELON_ASSERT(pool.capacity == test_capacity, "capacity == %zu", pool.capacity);
    MELON_ASSERT(pool.num_free_indices == test_capacity);

    return 0;
}
