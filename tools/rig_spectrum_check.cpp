// Determine whether each NAM rig already has a cab baked in by measuring its
// spectral tilt. Run white noise through the rig, then measure band RMS with
// bandpass filters. A real guitar cab rolls off steeply above ~5 kHz
// (presence/air 15-35 dB below the mids). A dry/amp-only capture keeps strong
// high-frequency energy (fizz) right up to 10 kHz+.
#include <cstdio>
#include <vector>
#include <cmath>
#include <filesystem>
#include "NAM/get_dsp.h"

struct BQ { double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    double p(double x){ double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y; } };

static BQ bandpass(double fs,double f,double Q){
    BQ q; double w=2*M_PI*f/fs,c=cos(w),s=sin(w),al=s/(2*Q);
    double a0=1+al; q.b0=al/a0; q.b1=0; q.b2=-al/a0; q.a1=-2*c/a0; q.a2=(1-al)/a0; return q; }

static double db(double x){ return 20.0*std::log10(x<1e-12?1e-12:x); }

int main(int argc,char**argv){
    const char* files[3]={"assets/rig_bite.nam","assets/rig_body.nam","assets/rig_edge.nam"};
    const double SR=48000.0; const int N=2048;
    const double centers[]={100,250,500,1000,2000,4000,6000,8000,12000};
    const int NB=sizeof(centers)/sizeof(centers[0]);

    for(int r=0;r<3;++r){
        auto dsp=nam::get_dsp(std::filesystem::path(files[r]));
        if(!dsp){ printf("FAIL %s\n",files[r]); continue; }
        dsp->Reset(SR,N);
        std::vector<BQ> bp; for(int b=0;b<NB;++b) bp.push_back(bandpass(SR,centers[b],4.0));
        std::vector<double> acc(NB,0); long n=0;
        unsigned s=7; auto rnd=[&]{ s=s*1664525u+1013904223u; return ((double)s/4294967295.0)*2-1; };
        for(int blk=0; blk<300; ++blk){
            std::vector<double> in(N),out(N);
            for(int i=0;i<N;++i) in[i]=0.1*rnd();
            double* ip[1]={in.data()}; double* op[1]={out.data()};
            dsp->process(ip,op,N);
            if(blk<30) continue;
            for(int i=0;i<N;++i){ for(int b=0;b<NB;++b){ double y=bp[b].p(out[i]); acc[b]+=y*y; } ++n; }
        }
        // normalise to the 1 kHz band
        int ref=3; double refrms=std::sqrt(acc[ref]/n);
        printf("\n%s  (relative to 1 kHz)\n",files[r]);
        for(int b=0;b<NB;++b){ double rms=std::sqrt(acc[b]/n);
            printf("  %5.0f Hz : %+6.1f dB\n",centers[b],db(rms)-db(refrms)); }
        double airVsMid = db(std::sqrt(acc[7]/n)) - db(std::sqrt(acc[4]/n)); // 8k vs 2k
        printf("  => 8kHz vs 2kHz = %+.1f dB  %s\n", airVsMid,
            airVsMid < -15.0 ? "*** CAB BAKED IN (steep HF rolloff) ***"
                             : "DRY / amp-only (lots of HF -> wants a cab IR)");
    }
    return 0;
}
