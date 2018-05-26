#include <engine/tz_core.h>
#include <engine/tz_error.h>

int main()
{
  const size_t test_capacity = 12;
  tz_pool pool;
  tz_create_pool(&pool, test_capacity, tz_default_cb_allocator());

  TZ_ASSERT(pool.free_indices != NULL);
  TZ_ASSERT(pool.capacity == test_capacity, "capacity == %zu", pool.capacity);
  TZ_ASSERT(pool.num_free_indices == test_capacity);

  return 0;
}
