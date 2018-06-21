#include <melon/core.hpp>
#include <melon/error.hpp>

int main()
{
    const size_t test_capacity = 12;
    melon::pool pool;
    melon::create_pool(&pool, test_capacity, melon::default_cb_allocator());

    TZ_ASSERT(pool.free_indices != NULL);
    TZ_ASSERT(pool.capacity == test_capacity, "capacity == %zu", pool.capacity);
    TZ_ASSERT(pool.num_free_indices == test_capacity);

    return 0;
}
