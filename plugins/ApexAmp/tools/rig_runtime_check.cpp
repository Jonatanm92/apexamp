// Standalone runtime proof: mirrors NamEngine's load path exactly.
// Reads each .nam as raw bytes -> nlohmann::json::parse -> nam::get_dsp(json)
// -> Reset -> process a sine -> report RMS. Confirms rigs actually make sound.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include "NAM/get_dsp.h"

static std::string readAll(const char* p){
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

int main(){
    const char* files[3] = {
        "assets/rig_bite.nam", "assets/rig_body.nam", "assets/rig_edge.nam" };
    const double SR = 48000.0; const int N = 512;
    constexpr double kPi = 3.14159265358979323846;
    int failures = 0;

    for (int r = 0; r < 3; ++r){
        std::string bytes = readAll(files[r]);
        if (bytes.empty()){ printf("FAIL: could not read %s\n", files[r]); ++failures; continue; }
        std::unique_ptr<nam::DSP> dsp;
        try {
            auto j = nlohmann::json::parse(bytes);   // same as BinaryData path
            dsp = nam::get_dsp(j);
        } catch (const std::exception& e){
            printf("FAIL: %s threw on load: %s\n", files[r], e.what()); ++failures; continue;
        }
        if (!dsp){ printf("FAIL: %s -> null dsp\n", files[r]); ++failures; continue; }

        dsp->Reset(SR, N);
        const double loud = dsp->HasLoudness() ? dsp->GetLoudness() : -18.0;

        // 2 s of a 110 Hz sine at -12 dBFS through the rig
        double acc = 0.0, pk = 0.0; long n = 0; bool finite = true;
        const int blocks = (int)(SR * 2.0 / N);
        for (int b = 0; b < blocks; ++b){
            std::vector<double> in(N), out(N);
            for (int i = 0; i < N; ++i){
                double t = (double)(b*N + i) / SR;
                in[i] = 0.25 * std::sin(2.0*kPi*110.0*t);
            }
            double* ip[1] = { in.data() }; double* op[1] = { out.data() };
            dsp->process(ip, op, N);
            if (b < 8) continue; // warmup
            for (int i = 0; i < N; ++i){
                double y = out[i];
                if (!std::isfinite(y)) finite = false;
                pk = std::max(pk, std::fabs(y)); acc += y*y; ++n;
            }
        }
        double rms = std::sqrt(acc / std::max(1L, n));
        bool ok = finite && rms > 1e-5;
        printf("%-20s loaded OK  loudness=%.2f LUFS  peak=%.3f  rms=%.4f  %s\n",
               files[r], loud, pk, rms, ok ? "*** AUDIO OK ***" : "!!! SILENT/BAD !!!");
        if (!ok) ++failures;
    }
    printf("\n%s\n", failures == 0 ? "PASS: all 3 rigs load and produce audio"
                                   : "FAIL: some rigs silent/failed");
    return failures;
}
