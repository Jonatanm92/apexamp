// Measures the real cab-induced level drop and the makeup gain NamEngine will
// apply, so we can confirm cab on/off end up level-matched. Replicates the
// engine math: white noise -> rig (loudness-matched) -> convolve with IR.
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <memory>
#include <algorithm>
#include "NAM/get_dsp.h"

static std::string readAll(const char* p){
    std::ifstream f(p, std::ios::binary); std::stringstream ss; ss<<f.rdbuf(); return ss.str();
}

// Minimal WAV reader: PCM 16/24/32-bit + IEEE float 32, returns mono-summed floats.
static bool readWavMono(const char* path, std::vector<float>& out){
    std::string b = readAll(path);
    if (b.size() < 44) return false;
    auto u16=[&](size_t o){ return (uint16_t)((uint8_t)b[o] | ((uint8_t)b[o+1]<<8)); };
    auto u32=[&](size_t o){ return (uint32_t)((uint8_t)b[o]|((uint8_t)b[o+1]<<8)|((uint8_t)b[o+2]<<16)|((uint8_t)b[o+3]<<24)); };
    size_t pos=12; uint16_t fmt=1, ch=1, bits=16; size_t dataOff=0, dataLen=0;
    while (pos+8 <= b.size()){
        uint32_t id=u32(pos); uint32_t sz=u32(pos+4); size_t body=pos+8;
        if (id==0x20746d66){ fmt=u16(body); ch=u16(body+2); bits=u16(body+14); }
        else if (id==0x61746164){ dataOff=body; dataLen=sz; }
        pos = body + sz + (sz&1);
    }
    if (!dataOff || !ch) return false;
    const int bytes=bits/8; const size_t frames=dataLen/(bytes*ch);
    out.assign(frames,0.0f);
    for (size_t i=0;i<frames;++i){ double acc=0;
        for (int c=0;c<ch;++c){ size_t o=dataOff + (i*ch+c)*bytes; double v=0;
            if (fmt==3 && bits==32){ uint32_t r=u32(o); float f; std::memcpy(&f,&r,4); v=f; }
            else if (bits==16){ int16_t s=(int16_t)u16(o); v=s/32768.0; }
            else if (bits==24){ int32_t s=(uint8_t)b[o]|((uint8_t)b[o+1]<<8)|((uint8_t)b[o+2]<<16); if(s&0x800000)s-=0x1000000; v=s/8388608.0; }
            else if (bits==32){ int32_t s=(int32_t)u32(o); v=s/2147483648.0; }
            acc+=v; }
        out[i]=(float)(acc/ch);
    }
    return true;
}

static double rms(const std::vector<double>& v){ if(v.empty())return 0; double e=0; for(double x:v)e+=x*x; return std::sqrt(e/v.size()); }

int main(){
    const double SR=48000.0; const int L=16384;
    auto dsp = nam::get_dsp(std::filesystem::path("assets/rig_bite.nam"));
    if(!dsp){ printf("no rig\n"); return 1; }
    double loud = dsp->HasLoudness()?dsp->GetLoudness():-18.0;
    double mg = std::pow(10.0,(-18.0-loud)/20.0);
    dsp->Reset(SR,L);

    std::vector<double> x(L), y(L);
    uint32_t s=22695477u; auto rnd=[&]{ s=s*1664525u+1013904223u; return ((double)s/(double)0xFFFFFFFFu)*2.0-1.0; };
    for(int i=0;i<L;++i) x[i]=0.1*rnd();
    double* ip[1]={x.data()}; double* op[1]={y.data()};
    dsp->process(ip,op,L);
    for(int i=0;i<L;++i) y[i]*=mg;
    double dryRms=rms(y);

    const char* irs[3]={"assets/ir_ashen.wav","assets/ir_meshuggah.wav","assets/ir_pdi09.wav"};
    for(int f=0;f<3;++f){
        std::vector<float> ir; if(!readWavMono(irs[f],ir)){ printf("%s: read fail\n",irs[f]); continue; }
        int H=std::min((int)ir.size(),8192);
        double e=0; long cnt=0;
        for(int i=H;i<L;++i){ double acc=0; for(int k=0;k<H;++k) acc+=y[i-k]*(double)ir[k]; e+=acc*acc; ++cnt; }
        double wetRms = cnt? std::sqrt(e/cnt):0;
        double mk = wetRms>1e-9? dryRms/wetRms : 1.0;
        double mkC = std::min(64.0,std::max(0.0625,mk));
        printf("%-22s dryRMS=%.4f wetRMS=%.4f  cab drop=%.1f dB  makeup=%.2fx (%+.1f dB)%s\n",
               irs[f], dryRms, wetRms, 20*std::log10(wetRms/dryRms), mkC, 20*std::log10(mkC),
               (mk!=mkC)?"  [clamped]":"");
    }
    return 0;
}
