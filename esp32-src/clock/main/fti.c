// fti: fault-tolerant intersection
//
// https://github.com/oreparaz/fti
//
// (c) 2022 Oscar Reparaz <firstname.lastname@esat.kuleuven.be>

#include "fti.h"

fti_ret_t fti_add_sample(fti_ctx_t *ctx, uint64_t left, uint64_t right) {
    ctx->samples_left [ctx->size] = left;
    ctx->samples_right[ctx->size] = right;
    ctx->size++;
    return FTI_SUCCESS;
}

fti_ret_t fti_get_intersection(fti_ctx_t *ctx, uint64_t *out_left, uint64_t *out_right) {
    if (ctx->faulty >= ctx->size) {
        return FTI_ERROR_INVALID_FAULTY;
    }
    fti_insertion_sort(ctx->samples_left, ctx->size, FTI_SORT_ASC);
    fti_insertion_sort(ctx->samples_right, ctx->size, FTI_SORT_DESC);
    *out_left  = ctx->samples_left [ctx->faulty];
    *out_right = ctx->samples_right[ctx->faulty];
    return FTI_SUCCESS;
}

void fti_insertion_sort(uint32_t *a, const size_t n, fti_sortdir_t sd) {
    for (size_t i=1; i < n; ++i) {
        uint32_t key = a[i];
        size_t j = i;
        while ((j > 0) && ((sd == FTI_SORT_ASC) ? (key < a[j - 1]) : (key >= a[j - 1]))) {
            a[j] = a[j - 1];
            --j;
        }
        a[j] = key;
    }
}