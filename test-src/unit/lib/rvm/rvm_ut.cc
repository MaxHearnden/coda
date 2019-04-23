#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <rvm/rvm.h>
#include <unistd.h> // fork()
#include <sys/wait.h> // WIFEXITED, etc.

#ifdef __cplusplus
}
#endif

#include <testing/memory.h>

namespace
{
class RvmTest : public ::testing::Test {
};

class RvmDeathTest : public ::testing::Test {
public:
    char *tmp_data;
    int tmp_data_size;
    static char *data_dev;

    void map_and_populate_region();
};

char *RvmDeathTest::data_dev = "rvm_test.data";

TEST(RvmTest, alloc_options)
{
    rvm_options_t *init_options = rvm_malloc_options();
    ASSERT_TRUE(init_options);

    rvm_free_options(init_options);

    /* rvm_malloc_options will initialize some structures
     * under the hood */
    ASSERT_EQ(rvm_terminate(), RVM_EINIT);
}

TEST(RvmTest, terminate)
{
    ASSERT_EQ(rvm_terminate(), RVM_EINIT);
}

RVM_TEST_F(RvmDeathTest, init_default)
{
    EXPECT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);
    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST_F(RvmDeathTest, init_minimal)
{
    const int TRUNCATE_VAL      = 30; /* truncate threshold */
    rvm_options_t *init_options = rvm_malloc_options();
    EXPECT_TRUE(init_options);

    init_options->truncate = TRUNCATE_VAL;
    init_options->flags |= RVM_ALL_OPTIMIZATIONS;
    init_options->flags |= RVM_MAP_PRIVATE;

    EXPECT_EQ(rvm_initialize(RVM_VERSION, init_options), RVM_SUCCESS);

    rvm_free_options(init_options);
    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST_F(RvmDeathTest, init_default_alloc)
{
    rvm_region_t *rvm_reg = NULL;
    ASSERT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);
    rvm_reg = rvm_malloc_region();
    ASSERT_TRUE(rvm_reg);
    rvm_free_region(rvm_reg);

    ASSERT_EQ(rvm_terminate(), RVM_SUCCESS);
}

RVM_TEST_F(RvmDeathTest, create_log_dev)
{
    rvm_return_t ret_val        = 0;
    rvm_options_t *init_options = rvm_malloc_options();
    rvm_offset_t offset;

    ASSERT_EQ(rvm_initialize(RVM_VERSION, NULL), RVM_SUCCESS);

    remove("rvm_test.log");
    init_options->log_dev  = "rvm_test.log";
    init_options->truncate = 30;
    init_options->flags |= RVM_ALL_OPTIMIZATIONS;
    // init_options->flags |= RVM_MAP_PRIVATE;
    offset = RVM_MK_OFFSET(0, 4096);

    ret_val = rvm_create_log(init_options, &offset, 0644);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    printf("%s %d\n", __FILE__, __LINE__);

    ret_val = rvm_set_options(init_options);
    EXPECT_EQ(ret_val, RVM_SUCCESS);

    printf("%s %d\n", __FILE__, __LINE__);

    rvm_free_options(init_options);

    printf("%s %d\n", __FILE__, __LINE__);
    EXPECT_EQ(rvm_terminate(), RVM_SUCCESS);
}

} // namespace