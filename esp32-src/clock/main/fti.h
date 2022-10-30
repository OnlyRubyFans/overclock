// fti: fault-tolerant intersection
//
// https://github.com/oreparaz/fti
//
// (c) 2022 Oscar Reparaz <firstname.lastname@esat.kuleuven.be>

#ifndef FTI_H
#define FTI_H

#include <stddef.h>
#include <stdint.h>

typedef struct fti_ctx_t {
    uint32_t *samples_left;
    uint32_t *samples_right;
    uint32_t size;
    uint32_t faulty;
} fti_ctx_t;

typedef enum {
    FTI_SUCCESS = 0,
    FTI_ERROR,
    FTI_ERROR_INVALID_FAULTY,
} fti_ret_t;

typedef enum {
    FTI_SORT_DESC,
    FTI_SORT_ASC,
} fti_sortdir_t;

#ifdef __cplusplus
extern "C" {
#endif

fti_ret_t fti_add_sample(fti_ctx_t *ctx, uint64_t left, uint64_t right);
fti_ret_t fti_get_intersection(fti_ctx_t *ctx, uint64_t *out_left, uint64_t *out_right);

void fti_insertion_sort(uint32_t *a, const size_t n, fti_sortdir_t sd);

#ifdef __cplusplus
}
#endif

#endif //FTI_H
