#include <math.h>

#define RESPONSE_LENGTH 32

void finger_exercise(short *signal, short *answer, int signal_length);
struct s_render_3D_dbentry;
struct s_render_3D_dbentry *render_3D_init(void);
void render_3D_free(struct s_render_3D_dbentry **data);
void render_3D(rx_queue_element_struct *el, int no_channels);
void convolve(short *signal, short *answer, double *overlap, double *response, int response_length, int signal_length);
