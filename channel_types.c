#include "config_unix.h"
#include "config_win32.h"

#include "codec_types.h"
#include "channel_types.h"

#include "util.h"
#include "debug.h"

int 
channel_data_create(channel_data **ppcd, int nelem)
{
        channel_data *pcd;
        int i;

        *ppcd = NULL;
        pcd = (channel_data*)block_alloc(sizeof(channel_data));

        if (pcd) {
                for(i = 0; i < nelem; i++) {
                        pcd->elem[i] = (channel_unit*)block_alloc(sizeof(channel_unit));
                        if (pcd->elem[i] == NULL) {
                                pcd->nelem = i;
                                channel_data_destroy(&pcd);
                                return FALSE;
                        }
                        memset(pcd->elem[i], 0, sizeof(channel_unit));
                }
                pcd->nelem = nelem;
                *ppcd = pcd;
                return TRUE;
        }
        return FALSE;
}

void
channel_data_destroy(channel_data **ppcd)
{
        channel_data *pcd;
        channel_unit *pcu;
        int i;

        pcd = *ppcd;
        assert(pcd != NULL);

        for(i = 0; i < pcd->nelem; i++) {
                pcu = pcd->elem[i];
                if (pcu->data) {
                        block_free(pcu->data, pcu->data_len);
                        pcu->data_len = 0;
                }
                assert(pcu->data_len == 0);
                block_free(pcu, sizeof(channel_data));
        }
        block_free(pcd, sizeof(channel_data));
        *ppcd = NULL;
}

int  
media_data_create(media_data **ppmd, int nrep)
{
        media_data *pmd;
        int i;

        *ppmd = NULL;
        pmd   = (media_data*)block_alloc(sizeof(media_data));
        
        if (pmd) {
                for(i = 0; i < nrep; i++) {
                        pmd->rep[i] = block_alloc(sizeof(coded_unit));
                        if (pmd->rep[i] == NULL) {
                                pmd->nrep = i;
                                media_data_destroy(&pmd);
                                return FALSE;
                        }
                        memset(pmd->rep[i], 0, sizeof(pmd->rep[i]));
                }
                pmd->nrep = nrep;
                *ppmd = pmd;
                return TRUE;
        }
        return FALSE;
}

void 
media_data_destroy(media_data **ppmd)
{
        media_data *pmd;
        coded_unit *pcu;
        int         i;
        
        pmd = *ppmd;
        assert(pmd != NULL);

        for(i = 0; i < pmd->nrep; i++) {
                pcu = pmd->rep[i];
                if (pcu->state) {
                        block_free(pcu->state, pcu->state_len);
                        pcu->state_len = 0;
                }
                assert(pcu->state_len == 0);
                if (pcu->state) {
                        block_free(pcu->data, pcu->data_len);
                        pcu->data_len = 0;
                }
                assert(pcu->data_len == 0);
                block_free(pcu, sizeof(channel_unit));
        }
        block_free(pmd, sizeof(media_data));
        *ppmd = NULL;
}
