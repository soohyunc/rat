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
        channel_unit *cu;
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
                *ppcd = pcd;
                return TRUE;
        }
        return FALSE;
}

void
channel_data_destroy(channel_data **cd)
{
        channel_unit *cu;
        
}

int  
media_data_create(media_data **m, int nrep)
{

}

void 
media_data_destroy(media_data **m)
{

}
