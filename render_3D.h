#include <math.h>

void finger_exercise(short *signal, short *answer, int signal_length);
struct s_render_3D_dbentry;
struct s_render_3D_dbentry *render_3D_init(void);
void render_3D_free(struct s_render_3D_dbentry **data);
void externalise(rx_queue_element_struct *el);
void convolve(short *signal, short *answer, double *overlap, double *response, int response_length, int signal_length);
