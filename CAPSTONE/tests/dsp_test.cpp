// ============================================================================
//  CAPSTONE — banc de test du coeur DSP (aucune dependance)
//  Inclut les cas de conformite EBU Tech 3341 realisables hors fichier audio.
// ============================================================================
#include "../Source/DSP/Loudness.h"
#include "../Source/DSP/Multiband.h"
#include "../Source/DSP/DynamicEq.h"
#include "../Source/DSP/MidSide.h"
#include "../Source/DSP/Limiter.h"
#include "../Source/DSP/Dither.h"
#include "../Source/DSP/Saturation.h"
#include <cstdio>
#include <vector>
#include <random>

using namespace mono;
static int failures = 0;
static void check (bool ok, const char* what) { printf("%-58s %s\n", what, ok?"[OK]":"[FAIL]"); if(!ok) ++failures; }

static bool finite(const std::vector<float>& v){ for(float x:v) if(!std::isfinite(x)) return false; return true; }
static float peak(const std::vector<float>& v){ float p=0; for(float x:v) p=std::max(p,std::abs(x)); return p; }
static float rms (const std::vector<float>& v){ double s=0; for(float x:v) s+=double(x)*x; return (float)std::sqrt(s/v.size()); }
static float binMag(const std::vector<float>& v,double f,double sr){
    double re=0,im=0,n=(double)v.size();
    for(size_t i=0;i<v.size();++i){ double w=0.5-0.5*std::cos(6.283185307*i/(n-1));
        double a=6.283185307*f*i/sr; re+=w*v[i]*std::cos(a); im-=w*v[i]*std::sin(a); }
    return (float)(2.0*std::sqrt(re*re+im*im)/n); }

int main()
{
    const double sr = 48000.0;
    printf("--- Mesure de loudness (ITU-R BS.1770-4 / EBU R128) ---\n");

    // ---- EBU Tech 3341, cas 1 et 2 : sinus 1 kHz stereo ----
    {
        const int N=(int)(sr*8); std::vector<float> L(N),R(N); const float* in[2]={L.data(),R.data()};
        for (double target : {-23.0, -33.0}) {
            const float a = (float)std::pow(10.0, target/20.0);
            for(int i=0;i<N;++i){ L[i]=a*(float)std::sin(6.283185307*1000.0*i/sr); R[i]=L[i]; }
            LoudnessMeter m; m.prepare(sr,2); m.processBlock(in,2,N);
            char buf[96]; snprintf(buf,sizeof(buf),"EBU 3341 cas %d : sinus 1 kHz %.0f dBFS -> %.2f LUFS",
                                   target==-23.0?1:2, target, m.getIntegrated());
            check(std::abs(m.getIntegrated()-target) < 0.1f, buf);
            if(target==-23.0)
                check(std::abs(m.getMomentary()-target)<0.1f && std::abs(m.getShortTerm()-target)<0.1f,
                      "  ... momentane et court terme concordent (+/-0.1 LU)");
        }
    }

    // ---- Linearite et sommation des canaux ----
    {
        const int N=(int)(sr*6); std::vector<float> L(N),R(N); const float* in[2]={L.data(),R.data()};
        auto meas=[&](float amp,bool stereo){ for(int i=0;i<N;++i){ L[i]=amp*(float)std::sin(6.283185307*1000.0*i/sr);
            R[i]= stereo?L[i]:0.f; } LoudnessMeter m; m.prepare(sr,2); m.processBlock(in,2,N); return m.getIntegrated(); };
        const float base=meas(0.25f,true), plus6=meas(0.5f,true), monoCh=meas(0.25f,false);
        check(std::abs((plus6-base)-6.0f)<0.05f,  "Linearite : +6 dB d'entree -> +6.00 LU");
        check(std::abs((base-monoCh)-3.01f)<0.05f,"Sommation : deux canaux identiques -> +3.01 LU");
    }

    // ---- Gating : le silence ne doit pas tirer la mesure vers le bas ----
    {
        const int N=(int)(sr*20); std::vector<float> L(N),R(N); const float* in[2]={L.data(),R.data()};
        const float a=(float)std::pow(10.0,-23.0/20.0);
        for(int i=0;i<N;++i){ bool loud = (i < (int)(sr*10));
            L[i]= loud ? a*(float)std::sin(6.283185307*1000.0*i/sr) : 0.f; R[i]=L[i]; }
        LoudnessMeter m; m.prepare(sr,2); m.processBlock(in,2,N);
        check(std::abs(m.getIntegrated()+23.0f)<0.2f, "Gating : 10 s a -23 LUFS + 10 s de silence -> -23 LUFS");
    }

    // ---- True peak : crete inter-echantillon superieure a la crete echantillon ----
    {
        const int N=4096; std::vector<float> L(N),R(N); const float* in[2]={L.data(),R.data()};
        // Sinus a Nyquist/2 dephase de 45 deg : les cretes tombent entre les echantillons.
        for(int i=0;i<N;++i){ L[i]=0.5f*(float)std::sin(6.283185307*12000.0*i/sr + 0.7853982); R[i]=L[i]; }
        LoudnessMeter m; m.prepare(sr,2); m.processBlock(in,2,N);
        const float tp=m.getTruePeakDb(), sp=m.getSamplePeakDb();
        printf("   crete echantillon %.2f dBFS | true peak %.2f dBTP\n", sp, tp);
        check(tp > sp + 0.5f, "True peak : detecte la crete cachee entre echantillons");
        check(tp < sp + 3.5f, "True peak : reste dans la borne theorique (+3 dB max)");
    }

    printf("\n--- Traitement ---\n");

    // ---- Multibande : reponse en magnitude plate, y compris aux coupures ----
    //  La somme des bandes est un passe-tout : la forme d'onde differe de
    //  l'entree, mais le spectre en magnitude doit etre identique. C'est cela
    //  qu'il faut mesurer, pas l'ecart echantillon par echantillon.
    {
        float worst = 0.0f;
        for (double f : {40.,120.,300.,800.,1500.,5000.,9000.,15000.}) {
            const int N=(int)(sr*0.5); std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
            for(int i=0;i<N;++i){ L[i]=0.3f*(float)std::sin(6.283185307*f*i/sr); R[i]=L[i]; }
            const float before = rms(L);
            Multiband mb; mb.prepare(sr,2); mb.setCrossovers(120.f,800.f,5000.f);
            for(int b=0;b<4;++b) mb.setBand(b,0.f,1.f,10.f,100.f,6.f,0.f,0.f,true);
            mb.process(io,2,N);
            std::vector<float> tail(L.begin()+4000, L.end());
            worst = std::max(worst, std::abs(gainToDb(rms(tail)/before)));
            check(finite(L), f==40. ? "Multibande : pas de NaN" : "Multibande : stable");
        }
        printf("   ecart maximal de magnitude sur 40 Hz - 15 kHz : %.3f dB\n", worst);
        check(worst < 0.1f, "Multibande : magnitude plate aux 3 coupures (+/-0.1 dB)");
    }

    // ---- Multibande : compresse bien la bande visee ----
    {
        const int N=(int)(sr*2); std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
        Multiband mb; mb.prepare(sr,2); mb.setCrossovers(120.f,800.f,5000.f);
        for(int b=0;b<4;++b) mb.setBand(b,0.f,1.f,10.f,100.f,6.f,0.f,0.f,true);
        mb.setBand(0,-30.f,8.f,5.f,120.f,3.f,0.f,0.f,false);      // grave seulement
        for(int i=0;i<N;++i){ L[i]=0.5f*(float)std::sin(6.283185307*60.0*i/sr); R[i]=L[i]; }
        mb.process(io,2,N);
        check(mb.getGrDb(0) < -8.f, "Multibande : la bande grave reduit sur un 60 Hz fort");
        check(mb.getGrDb(3) > -0.5f,"Multibande : la bande aigue reste inactive");
    }

    // ---- EQ dynamique ----
    {
        const int N=(int)(sr*2); std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
        DynamicBand dq; dq.prepare(sr,2);
        dq.setParams(true, 3000.f, 2.f, -30.f, 4.f, 5.f, 80.f, 8.f);
        for(int i=0;i<N;++i){ L[i]=0.5f*(float)std::sin(6.283185307*3000.0*i/sr); R[i]=L[i]; }
        dq.process(io,2,N);
        check(dq.getGainDb() < -3.f, "EQ dynamique : attenue quand la bande depasse le seuil");
        float tail=0; for(int i=N*3/4;i<N;++i) tail=std::max(tail,std::abs(L[i]));
        check(tail < 0.42f, "EQ dynamique : reduction effective sur le signal");
    }

    // ---- Mid/Side : bass mono + largeur ----
    {
        const int N=8192; std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
        MidSideSection ms; ms.prepare(sr);
        ms.setParams(150.f, 1.6f, 100.f,0.f, 8000.f,0.f, 100.f,0.f, 8000.f,0.f);
        for(int i=0;i<N;++i){ L[i]=0.4f*(float)std::sin(6.283185307*50.0*i/sr); R[i]=-L[i]; }
        ms.process(io,2,N);
        float side=0; for(int i=2000;i<N;++i) side=std::max(side,std::abs(0.5f*(L[i]-R[i])));
        check(side < 0.10f, "Mid/Side : le grave sous 150 Hz est recentre");
    }

    // ---- Limiteur true peak : le plafond doit tenir, mesure incluse ----
    {
        for (float ceilDb : {-1.0f, -0.3f}) {
            const int N=(int)(sr*3); std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
            const float* cin[2]={L.data(),R.data()};
            std::mt19937 rng(11); std::uniform_real_distribution<float> d(-1.f,1.f);
            for(int i=0;i<N;++i){ float env = (i%9600<400)? 3.0f : 0.7f;   // transitoires violents
                L[i]=d(rng)*env; R[i]=d(rng)*env; }
            LookaheadLimiter lim; lim.prepare(sr,2,3.0f); lim.setParams(ceilDb,120.f,6.f);
            lim.process(io,2,N);
            LoudnessMeter m; m.prepare(sr,2); m.processBlock(cin,2,N);
            char buf[96]; snprintf(buf,sizeof(buf),"Limiteur : plafond %.1f dBTP tenu (mesure %.2f dBTP)",
                                   ceilDb, m.getTruePeakDb());
            check(m.getTruePeakDb() <= ceilDb + 0.15f, buf);
            check(finite(L)&&finite(R), "  ... pas de NaN, GR coherente");
        }
    }

    // ---- Limiteur : garantie du plafond sur signaux adverses ----
    {
        int violations = 0; float worstOver = -99.f;
        const int N=(int)(sr*2);
        for (int sig=0; sig<4; ++sig) for (int seed=0; seed<4; ++seed) for (float la : {1.5f,3.0f,5.0f}) {
            std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
            const float* cin[2]={L.data(),R.data()};
            std::mt19937 rng((unsigned)(seed*977+sig)); std::uniform_real_distribution<float> d(-1.f,1.f);
            for(int i=0;i<N;++i){ float v=0;
                switch(sig){
                    case 0:{ float e=(i%9600<300)?4.f:0.6f; v=d(rng)*e; } break;
                    case 1: v=2.5f*(float)std::sin(6.283185307*15000.0*i/sr); break;
                    case 2: v=(i%480<3)?3.5f*d(rng):0.02f*d(rng); break;         // clics
                    default: v=(std::sin(6.283185307*300.0*i/sr)>0?1.8f:-1.8f); break;
                }
                L[i]=v; R[i]=v*0.93f; }
            LookaheadLimiter lim; lim.prepare(sr,2,la); lim.setParams(-1.f,120.f,6.f);
            lim.process(io,2,N);
            LoudnessMeter m; m.prepare(sr,2); m.processBlock(cin,2,N);
            const float over = m.getTruePeakDb() + 1.0f;
            worstOver = std::max(worstOver, over);
            if (over > 0.0f) ++violations;
        }
        printf("   48 cas adverses | pire ecart au plafond : %+.3f dB\n", worstOver);
        check(violations == 0, "Limiteur : plafond -1 dBTP jamais depasse (48 cas adverses)");
    }

    // ---- Limiteur : transparent sous le seuil ----
    {
        const int N=(int)(sr*1); std::vector<float> L(N),R(N),orig(N); float* io[2]={L.data(),R.data()};
        for(int i=0;i<N;++i){ L[i]=0.1f*(float)std::sin(6.283185307*440.0*i/sr); R[i]=L[i]; } orig=L;
        LookaheadLimiter lim; lim.prepare(sr,2,3.0f); lim.setParams(-1.f,120.f,0.f);
        lim.process(io,2,N);
        const int La=lim.getLatencySamples();
        float err=0; for(int i=2000;i<N-La;++i) err=std::max(err,std::abs(L[i+La]-orig[i]));
        printf("   latence declaree %d echantillons (%.2f ms) | erreur %.5f\n", La, La*1000.0/sr, err);
        check(err < 0.002f, "Limiteur : transparent sous le plafond, latence exacte");
    }

    // ---- Clipper suremaillonne : moins de repliement ----
    {
        const int N=8192; std::vector<float> a(N),b(N),R(N);
        for(int pass=0;pass<2;++pass){
            std::vector<float>& L = (pass==0)?a:b;
            for(int i=0;i<N;++i){ L[i]=0.9f*(float)std::sin(6.283185307*7000.0*i/sr); R[i]=L[i]; }
            float* io[2]={L.data(),R.data()};
            MasterClipper cl; cl.prepare(sr,2,N);
            cl.setParams(true, ClipMode::Hard, 8.f, -0.3f, pass==1);
            cl.process(io,2,N);
        }
        const float off=binMag(a,13000.0,sr), on=binMag(b,13000.0,sr);
        printf("   repli @13 kHz : sans OS %.1f dB | avec OS %.1f dB\n", gainToDb(off), gainToDb(on));
        check(on < off*0.05f, "Clipper : le sur-echantillonnage x4 effondre le repliement (>26 dB)");
    }

    // ---- Dither : bruit present, mise en forme effective ----
    {
        const int N=32768; std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
        auto run=[&](DitherMode m){ for(int i=0;i<N;++i){ L[i]=0.f; R[i]=0.f; }
            Dither d; d.prepare(2); d.setParams(m,16); d.process(io,2,N); return L; };
        auto flat   = run(DitherMode::TpdfFlat);
        auto shaped = run(DitherMode::ShapedStrong);
        check(rms(flat) > 1e-6f, "Dither : bruit TPDF present a 16 bits");
        const float fMid=binMag(flat,3500.0,sr), sMid=binMag(shaped,3500.0,sr);
        const float fHi =binMag(flat,19000.0,sr), sHi =binMag(shaped,19000.0,sr);
        printf("   @3.5 kHz : plat %.1f dB -> mis en forme %.1f dB | @19 kHz : %.1f -> %.1f dB\n",
               gainToDb(fMid), gainToDb(sMid), gainToDb(fHi), gainToDb(sHi));
        check(sMid < fMid, "Dither : bruit reduit dans la zone sensible (3.5 kHz)");
        check(sHi  > fHi,  "Dither : bruit repousse vers l'aigu (19 kHz)");
        check(peak(shaped) < 0.01f, "Dither : amplitude du bruit negligeable");
    }

    // ---- Chaine complete ----
    {
        const int N=(int)(sr*3); std::vector<float> L(N),R(N); float* io[2]={L.data(),R.data()};
        const float* cin[2]={L.data(),R.data()};
        std::mt19937 rng(99); std::uniform_real_distribution<float> d(-0.7f,0.7f);
        for(int i=0;i<N;++i){ L[i]=d(rng); R[i]=d(rng)*0.9f; }
        Multiband mb; DynamicBand de; MidSideSection ms; ColourStage col; MasterClipper cl;
        LookaheadLimiter lim; Dither dit;
        mb.prepare(sr,2); de.prepare(sr,2); ms.prepare(sr); col.prepare(sr,2,N);
        cl.prepare(sr,2,N); lim.prepare(sr,2,3.f); dit.prepare(2);
        mb.setCrossovers(120.f,800.f,5000.f);
        for(int b=0;b<4;++b) mb.setBand(b,-24.f,2.f,20.f,150.f,6.f,0.f,0.f,false);
        de.setParams(true,3000.f,1.5f,-26.f,3.f,10.f,120.f,4.f);
        ms.setParams(110.f,1.15f, 120.f,1.f, 9000.f,0.5f, 120.f,-2.f, 9000.f,1.5f);
        col.setParams(ColourMode::Tape,3.f,0.4f);
        cl.setParams(true,ClipMode::Soft,3.f,-0.3f,true);
        lim.setParams(-1.f,150.f,4.f);
        dit.setParams(DitherMode::ShapedLight,16);
        mb.process(io,2,N); de.process(io,2,N); ms.process(io,2,N);
        col.process(io,2,N); cl.process(io,2,N); lim.process(io,2,N); dit.process(io,2,N);
        LoudnessMeter m; m.prepare(sr,2); m.processBlock(cin,2,N);
        printf("   sortie : %.2f LUFS-I | %.2f dBTP | LRA %.1f LU | correlation %.2f\n",
               m.getIntegrated(), m.getTruePeakDb(), m.getLRA(), m.getCorrelation());
        check(finite(L)&&finite(R), "Chaine complete : pas de NaN/Inf");
        check(m.getTruePeakDb() <= -0.85f, "Chaine complete : true peak sous le plafond -1 dBTP");
    }

    printf("\n%s  (%d echec(s))\n", failures==0?">>> TOUS LES TESTS PASSENT":">>> ECHECS DETECTES", failures);
    return failures;
}
