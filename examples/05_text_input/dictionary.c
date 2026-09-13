#include "dictionary.h"
#include "saan_dict_blob.h"
int text_dictionary_open(jdict_t *d) {
    if (jdict_open(d, saan_dict_blob, sizeof(saan_dict_blob)) != 0) return 0;
    return d->n_entries == 44000 && d->lsize == 1377 && d->rsize == 1377 &&
           d->matrix_q && d->matrix_rmap && d->matrix_cmap && d->char_runs && d->unk;
}
