#include "config_unix.h"
#include "config_win32.h"
#include "ts.h"

#define ITERS 100000000
#define MARK  (ITERS/100)

u_int32 f[] = {8000, 11025, 16000, 22050, 24000, 32000, 40000, 44100, 48000, 90000};

#define NUM_F (sizeof(f)/sizeof(u_int32))

int 
main()
{
        ts_t a, b, c, dac;
        int ib, i, fa, fb;
        u_int32 ta, tb;

        for(ib = 0; ib < ITERS; ib += MARK) {
                for(i = ib; i < ib+MARK; i++) {
                        fa = f[random() % NUM_F];
                        fb = f[random() % NUM_F];
                        ta = (u_int32)random();
                        tb = (u_int32)random();
                        
                        a = ts_map32(fa,ta);
                        b = ts_map32(fb,tb);
                        c = ts_sub(a, b);
                        
                        assert(ts_valid(a));
                        assert(ts_valid(b));
                        assert(ts_valid(c));
                        
                        dac = ts_sub(a,c);
                        assert(ts_eq(dac,b));
                }
                printf("."); fflush(stdout);
        }
        fprintf(stderr, "\n");
        return TRUE;
}
