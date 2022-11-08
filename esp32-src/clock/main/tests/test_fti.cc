#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "byte_vec.h"
#include "../fti.h"

TEST_CASE("test insertion sort") {
    fti_sample_t a[4] = {4,2,3,1};
    fti_insertion_sort(a, 4, FTI_SORT_ASC);
    REQUIRE(byte_vec_t(a, a+4) == byte_vec_t{1, 2, 3, 4});

    fti_sample_t b[4] = {4,2,3,1};
    fti_insertion_sort(b, 4, FTI_SORT_DESC);
    REQUIRE(byte_vec_t(b, b+4) == byte_vec_t{4,3,2,1});

    fti_sample_t c[5] = {4,2,3,5,1};
    fti_insertion_sort(c, 5, FTI_SORT_ASC);
    REQUIRE(byte_vec_t(c, c+5) == byte_vec_t{1, 2, 3, 4, 5});
}

TEST_CASE("test fti") {
    fti_ctx_t ctx = {
            .samples_left = (fti_sample_t *)calloc(6, sizeof(fti_sample_t)),
            .samples_right = (fti_sample_t *)calloc(6, sizeof(fti_sample_t)),
            .faulty = 1,
    };
    fti_sample_t left, right;
    fti_add_sample(&ctx, 1, 10);
    fti_add_sample(&ctx, 2, 20);
    fti_add_sample(&ctx, 3, 30);
    fti_add_sample(&ctx, 4, 40);
    fti_add_sample(&ctx, 5, 50);
    fti_add_sample(&ctx, 6, 60);

    REQUIRE(FTI_SUCCESS == fti_get_intersection(&ctx, &left, &right));
    REQUIRE(left == 2);
    REQUIRE(right == 50);

    free(ctx.samples_right);
    free(ctx.samples_left);
}

TEST_CASE("test fti unsorted") {
    fti_ctx_t ctx = {
            .samples_left = (fti_sample_t *)calloc(6, sizeof(fti_sample_t)),
            .samples_right = (fti_sample_t *)calloc(6, sizeof(fti_sample_t)),
            .faulty = 1,
    };
    fti_sample_t left, right;
    fti_add_sample(&ctx, 6, 60);
    fti_add_sample(&ctx, 5, 50);
    fti_add_sample(&ctx, 4, 40);
    fti_add_sample(&ctx, 1, 10);
    fti_add_sample(&ctx, 2, 20);
    fti_add_sample(&ctx, 3, 30);

    REQUIRE(FTI_SUCCESS == fti_get_intersection(&ctx, &left, &right));
    REQUIRE(left == 2);
    REQUIRE(right == 50);

    free(ctx.samples_right);
    free(ctx.samples_left);
}
