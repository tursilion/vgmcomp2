// Hand converted by Tursi

#include <vector>
#include <string>
#include <algorithm>
#include <math.h>
using namespace std;
extern bool verbose, debug2;

// returns a vector of doubles for the samples (-1 to +1)
// wave is converted from 8 or 16 bit to floating point, and from stereo to mono
// Unlike the matlab version, we'll do the resample at the same time, since
// that will save a copy or two. So you know the return type and frequency.
// returns empty vector on failure
vector<double> wavread(const string &name, int newFreq) {
	unsigned char buf[44];
	vector<double> ret;

	// not intended to be all-encompassing, just intended to work
	FILE *fp = fopen(name.c_str(), "rb");
	if (NULL == fp) {
		printf("* can't open %s (code %d)\n", name.c_str(), errno);
		return ret;
	}

	if (1 != fread(buf, 44, 1, fp)) {
		fclose(fp);
		printf("* can't read WAV header on %s\n", name.c_str());
		return ret;
	}

	if (0 != memcmp(buf, "RIFF", 4)) {
		fclose(fp);
		printf("* not RIFF on %s\n", name.c_str());
		return ret;
	}

	if (0 != memcmp(&buf[8], "WAVE", 4)) {
		fclose(fp);
		printf("* not WAVE on %s\n", name.c_str());
		return ret;
	}

	if (0 != memcmp(&buf[12], "fmt ", 4)) {
		// technically this could be elsewhere, but that'd be unusual
		fclose(fp);
		printf("* fmt not as expected on %s\n", name.c_str());
		return ret;
	}

	// okay, we know that we have at least got a header
	if (0 != memcmp(&buf[20], "\x1\x0", 2)) {
		fclose(fp);
		printf("* not PCM on %s\n", name.c_str());
		return ret;
	}

	int chans = buf[22] + buf[23]*256;
	if ((chans < 1) || (chans > 2)) {
		fclose(fp);
		printf("* %d chans on %s (support 1-2)\n", chans, name.c_str());
		return ret;
	}

	int freq = buf[24] + buf[25]*256 + buf[26]*65536 + buf[27]*16777216;
	if ((freq < 1000) || (freq > 224000)) {
		fclose(fp);
		printf("Freq of %d on %s (support 1000-224000)\n", freq, name.c_str());
		return ret;
	}

	int bits = buf[34] + buf[35]*256;
	if ((bits != 8)&&(bits != 16)) {
		fclose(fp);
		printf("Bits are %d on %s (support 8,16)\n", bits, name.c_str());
		return ret;
	}

	if (0 != memcmp(&buf[36], "data", 4)) {
		// technically this could be elsewhere, but that'd be unusual
		fclose(fp);
		printf("* data not as expected on %s\n", name.c_str());
		return ret;
	}

	int bytes = buf[40] + buf[41]*256 + buf[42]*65536 + buf[43]*16777216;	// data size in bytes

    // check if it's sane! More than, say, 100MB is just too silly)
    if (bytes > 100*1024*1024) {
        printf("Wave file too large - cut it into pieces!\n");
        return ret;
    }
	
	// use that value to get our buffer before we scale it down to actual samples
    unsigned char *tmpBuf = (unsigned char*)malloc(bytes);


	// and read it - this works if the above did since we are stopped
	// right at the beginning of the data chunk
	if (fread(tmpBuf, 1, bytes, fp) != bytes) {
		printf("+ warning: failed to read all bytes from %s\n", name.c_str());
	}
	fclose(fp);

	// now convert bytes to samples
	int samples = bytes / (bits/8);	// by bytes per sample
	samples /= chans;		        // by number of channels - NOW it's samples

    if ((bits != 8)&&(bits != 16)) {
        printf("Wave must be 8 or 16 (LE) bits only (got %d).\n", bits);
        free(tmpBuf);
        return ret;
    }
    if ((chans < 1)||(chans > 2)) {
        printf("Wave must be 1 or 2 channels only (got %d)\n", chans);
        free(tmpBuf);
        return ret;
    }

    // convert to a set of floating point samples for the FFT
    // resample to newFreq as we go
    // original code inserted 2 frames of silence, so we will too
    int silence = int(round((2/60.0)*newFreq));
    int newsamples = int(((double)samples/freq) * newFreq) + silence;
    double samplestep = (double)freq/newFreq;
    ret.resize(newsamples);
    for (int idx=0; idx<silence; ++idx) {
        ret[idx] = 0;
    }

    double offset = 0;
    // resample with linear interpolation
    // also convert to mono float samples
    // only support mono or stereo, 8 or 16 bit
    for (int i=silence; i<newsamples; ++i) {
        unsigned char *p = tmpBuf + int(floor(offset)*chans*(bits/8));
        float sample = 0;
        if (bits == 16) {
            short s1,s2;
            s1 = *p + ((*(p+1))<<8);
            if (chans == 2) {
                p+=2;
                s1 += *p + ((*(p+1))<<8);
                s1 /= 2;
            }

            p+=2;
            s2 = *p + ((*(p+1))<<8);
            if (chans == 2) {
                p+=2;
                s2 += *p + ((*(p+1))<<8);
                s2 /= 2;
            }

            // ratio is the percentage of s2 we take
            double ratio = offset - floor(offset);
            sample = float(((s1/32768.0)*(1-ratio)) + ((s2/32768.0)*(ratio)));
        } else {
            signed char s1,s2;
            s1 = *p;
            if (chans == 2) {
                p++;
                s1 += *p;
                s1 /= 2;
            }

            p++;
            s2 = *p;
            if (chans == 2) {
                p++;
                s2 += *p;
                s2 /= 2;
            }

            // ratio is the percentage of s2 we take
            double ratio = offset - floor(offset);
            sample = float(((s1/128.0)*(1-ratio)) + ((s2/128.0)*(ratio)));
        }
        ret[i] = sample;
        offset+=samplestep;
    }
    samples = newsamples;
    free(tmpBuf);

    if (debug2) {
        // write out the converted wave file to ensure it's correct
        // we'll write signed 16-bit for simplicity
        FILE *fp = fopen("samplesound.wav", "wb");
        if (NULL != fp) {
            fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x1\0\x1\0\x44\xac\0\0\x44\xac\0\0\x10\0\x10\0data\0\0\0\0",
                1,44,fp);
            for (int idx=0; idx<samples; ++idx) {
                short x = short(32767*ret[idx]);
                fwrite(&x, 2, 1, fp);
            }
            fclose(fp);
            fp = fopen("samplesound.wav", "rb+");
            if (NULL != fp) {
                int wavelen = samples*2;
                fseek(fp, 40, SEEK_SET);
                fputc(wavelen%0xff, fp);
                fputc((wavelen>>8)&0xff, fp);
                fputc((wavelen>>16)&0xff, fp);
                fputc((wavelen>>24)&0xff, fp);

                fseek(fp, 4, SEEK_SET);
                wavelen+=44;    // add size of header
                fputc(wavelen%0xff, fp);
                fputc((wavelen>>8)&0xff, fp);
                fputc((wavelen>>16)&0xff, fp);
                fputc((wavelen>>24)&0xff, fp);
                fclose(fp);
            }
        }
    }

	// and we're done
	return ret;
}

typedef struct FEATUREVECTOR {
    vector<double> vuvfea;  // voiced/unvoiced features
    vector<double> best;    // selected path
    vector<double> ff;      // pitch candidates
    vector<double> amp;     // pitch candidate amplitudes
    vector<double> medfx;   // median pitch
    vector<double> w;       // DP weights
    double dffact;          // df scale factor
    vector<double> hist;    // some kind of history
} SFV;
typedef struct PITCHPARAMETERS {
    double fstep;           // frequency resolution of initial spectrogram (hz)
    double fmax;            // maximum frequency of initial spectrogram (hz)
    double fres;            // bandwidth of initial spectrogram (hz)
    double fbanklo;         // low frequency limit of log v_filterbank (hz)
    double mpsmooth;        // width of smoothing filter for mean power
    double maxtranf;        // maximum value of tranf cost term
    double shortut;         // max utterance length to average power of entire utterance
    double pefact;          // shape factor in PEFAC filter
    double numopt;          // number of possible frequencies per frame
    vector<double> flim;    // range of possible fundamental frequencies (hz)
    vector<double> w;       // DP weights
    double rampk;           // constant for relative amplitude cost term
    double rampcz;          // relative amplitude cost for missing peak
    double tmf;             // median frequency smoothing interval (s)
    double tinc;            // default frame increment (s)
    char *sopt;             // spectrogram options
} SPP;

// this function makes the entire program GPL. Tell me again about how
// it's all about freedom. ;) See end of file for the mass of comments
// from the original source.
// V_FXPEFAC PEFAC pitch tracker [FX,TT,PV,FV]=(S,FS,TINC/*,M,PP*/)
// 
//  Input:   s(ns)      Speech signal
//           fs         Sample frequency (Hz)
//           tinc       Time increment between frames (s) [0.01]
//                      or [start increment end]
// mode(m) and pp are not implemented here
// 
//  Outputs: fx(nframe)     Estimated pitch (Hz)
//           tx(nframe)     Time at the centre of each frame (seconds).
//           pv(nframe)     Probability of the frame of being voiced
//           fv             structure containing feature vectors
//                            fv.vuvfea(nframe,2) = voiced/unvoiced GMM features
void fxpefac(vector<double> &s, int fs, double tinc, /* char m, SPP &pp, */
             vector<double> &fx, vector<double> &tx, vector<double> &pv, SFV &fv) {

persistent w_u m_u v_u w_v m_v v_v dpwtdef

% initialize persistent variables

 if ~numel(w_u)

 

     % voiced/unvoiced decision based on 2-element feature vector

     % (a) mean power of the frame's log-freq spectrum (normalized so its short-term average is LTASS)

     % (b) sum of the power in the first three peaks

     %===== VUV

     if nargin>3 && any(m=='x')

         fxpefac_g;     % read in GMM parameters

         fxpefac_w;     % read in Weights parameters

     else

         w_u=[0.1461799 0.3269458 0.2632178 0.02331986 0.06360947 0.1767271 ]';

 

         m_u=[13.38533 0.4199435 ;

              12.23505 0.1496836 ;

              12.76646 0.2581733 ;

              13.69822 0.6893078 ;

              9.804372 0.02786567 ;

              11.03848 0.07711229 ];

 

         v_u=reshape([0.4575519 0.002619074 0.002619074 0.01262138 ;

              0.7547719 0.008568089 0.008568089 0.001933864 ;

              0.5770533 0.003561592 0.003561592 0.00527957 ;

              0.3576287 0.01388739 0.01388739 0.04742106 ;

              0.9049906 0.01033191 0.01033191 0.0001887114 ;

              0.637969 0.009936445 0.009936445 0.0007082946 ]',[2 2 6]);

 

         w_v=[0.1391365 0.221577 0.2214025 0.1375109 0.1995124 0.08086066 ]';

 

         m_v=[15.36667 0.8961554 ;

              13.52718 0.4809653 ;

              13.95531 0.8901121 ;

              14.56318 0.6767258 ;

              14.59449 1.190709 ;

              13.11096 0.2861982 ];

 

         v_v=reshape([0.196497 -0.002605404 -0.002605404 0.05495016 ;

              0.6054919 0.007776652 0.007776652 0.01899244 ;

              0.5944617 0.0485788 0.0485788 0.03511229 ;

              0.3871268 0.0292966 0.0292966 0.02046839 ;

              0.3377683 0.02839657 0.02839657 0.04756354 ;

              1.00439 0.03595795 0.03595795 0.006737475 ]',[2 2 6]);

     end

     %===== PDP

     %     dfm = -0.4238; % df mean

     %     dfv = 3.8968; % df variance (although treated as std dev here)

     %     delta = 0.15;

     %     dflpso=[dfm 0.5/(log(10)*dfv^2) -log(2*delta/(dfv*sqrt(2*pi)))/log(10)]; % scale factor & offset for df pdf

     %     dpwtdef=[1.0000, 0.8250, 1.3064, 1.9863]; % default DP weights

     dpwtdef=[1.0000, 0.8250, 0.01868, 0.006773, 98.9, -0.4238]; % default DP weights

     %===== END

 

 end

 % Algorithm parameter defaults

 

 p.fstep=5;              % frequency resolution of initial spectrogram (Hz)

 p.fmax=4000;            % maximum frequency of initial spectrogram (Hz)

 p.fres = 20;            % bandwidth of initial spectrogram (Hz)

 p.fbanklo = 10;         % low frequency limit of log v_filterbank (Hz)

 p.mpsmooth = 21;       % width of smoothing filter for mean power

 % p.maxtranf = 1000;      % maximum value of tranf cost term

 p.shortut = 7;          % max utterance length to average power of entire utterance

 p.pefact = 1.8;         % shape factor in PEFAC filter

 p.numopt = 3;           % number of possible frequencies per frame

 p.flim = [60 400];      % range of feasible fundamental frequencies (Hz)

 p.w = dpwtdef;          % DP weights

 % p.rampk = 1.1;          % constant for relative-amplitude cost term

 % p.rampcz = 100;         % relative amplitude cost for missing peak

 p.tmf = 2;              % median frequency smoothing interval (s)

 p.tinc = 0.01;          % default frame increment (s)

 p.sopt = 'ilcwpf';      % spectrogram options

 

 % update parameters from pp argument

 

 if nargin>=5 && isstruct(pp)

     fnq=fieldnames(pp);

     for i=1:length(fnq)

         if isfield(p,fnq{i})

             p.(fnq{i})=pp.(fnq{i});

         end

     end

 end

 

 % Sort out input arguments

 if nargin>=3  && numel(tinc)>0

     p.tinc = tinc;   % 0.01 s between consecutive time frames

 end

 if nargin<4

     m='';

 end

 

 % Spectrogram of the mixture

 fmin = 0; fstep = p.fstep; fmax = p.fmax;

 fres = p.fres;  % Frequency resolution (Hz)

 [tx,f,MIX]=v_spgrambw(s,fs,fres,[fmin fstep fmax],[],p.tinc);

 nframes=length(tx);

 txinc=tx(2)-tx(1);  % actual frame increment

 %  ==== we could combine v_spgrambw and v_filtbankm into a single call to v_spgrambw or use fft directly ====

 % Log-frequency scale

 [trans,cf]=v_filtbankm(length(f),2*length(f)-1,2*f(end),p.fbanklo,f(end),'usl');

 O = MIX*trans'; % Original spectrum in Log-frequency scale

 

 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 % Amplitude Compression

 

 % Calculate alpha based on LTASS ratios

 ltass = v_stdspectrum(6,'p',cf); % uses an old version of the LTASS spectrum but we will need to recalculate the GMM if we update it

 auxf = [cf(1),(cf(1:end-1)+cf(2:end))./2,cf(end)];

 ltass = ltass.*diff(auxf);                  % weight by bin width

 

 % estimated ltass

 O = O.*repmat(diff(auxf),nframes,1);     % weight spectrum by bin width

 O1 = O;

 

 if tx(end)<p.shortut                        % if it is a short utterance

     eltass = mean(O,1);                     % mean power per each frequency band

     eltass = smooth(eltass,p.mpsmooth);     % smooth in log frequency

     eltass= eltass(:).';                    % force a row vector

 

     % Linear AC

     alpha = (ltass)./(eltass);

     alpha = alpha(:).';

     alpha = repmat(alpha,nframes,1);

     O = O.*alpha;                           % force O to have an average LTASS spectrum

 

     % ==== should perhaps exclude the silent portions ***

 else                                        % long utterance

 

     tsmo = 3; % time smoothing over 3 sec

     stt = round(tsmo/txinc);

     eltass = timesm(O,stt);

     eltass = smooth(eltass,p.mpsmooth);     % filter in time and log frequency

 

     % Linear AC

     alpha = repmat(ltass,nframes,1)./(eltass);

     O = O.*alpha;

 

 end

 

 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 % Create the filter to detect the harmonics

 ini = find(cf>3*cf(1));

 sca = cf/cf(ini(1)); % bin frequencies start at approximately 0.33 with sca(ini(1))=1 exactly

 

 % Middle

 sca = sca(sca<10.5 & sca>0.5);  % restrict to 0.5 - 10.5 times fundamental

 

 sca1 = sca;

 filh = 1./(p.pefact-cos(2*pi*sca1));

 filh = filh - sum(filh(1:end).*diff([sca1(1),(sca1(1:end-1)+sca1(2:end))./2,sca1(end)]))/sum(diff([sca1(1),(sca1(1:end-1)+sca1(2:end))./2,sca1(end)]));

 

 posit = find(sca>=1);  % ==== this should just equal ini(1) ====

 negat = find(sca<1);

 numz = length(posit)-1-length(negat);

 filh = filh./max(filh);

 filh = [zeros(1,numz) filh]; % length is always odd with central value = 1

 

 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 % Filter the log-frequency scaled spectrogram

 B = imfilter(O,filh);  % does a convolution with zero lag at centre of filh

 

 % Feasible frequency range

 numopt = p.numopt; % Number of possible fundamental frequencies per frame

 flim = p.flim;

 pfreq = find(cf>flim(1) & cf<flim(2)); % flim = permitted fx range = [60 400]

 ff = zeros(nframes,numopt);

 amp = zeros(nframes,numopt);

 for i=1:nframes

     [pos,peak]=v_findpeaks(B(i,pfreq),[],5/(cf(pfreq(2))-cf(pfreq(1)))); % min separation = 5Hz @ fx=flim(1) (could pre-calculate) ====

     if numel(pos)

         [peak,ind]=sort(peak,'descend');

         pos = pos(ind);                     % indices of peaks in the B array

         posff = cf(pfreq(pos));             % frequencies of peaks

         fin = min(numopt,length(posff));

         ff(i,1:fin)=posff(1:fin);           % save both frequency and amplitudes

         amp(i,1:fin)=peak(1:fin);

     end

 end

 

 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 % Probabilitly of the frame of being voiced

 

 % voiced/unvoiced decision based on 2-element feature vector

 % (a) mean power of the frame's log-freq spectrum (normalized so its short-term average is LTASS)

 % (b) sum of the power in the first three peaks

 

 pow = mean(O,2);

 

 vuvfea = [log(pow) 1e-3*sum(amp,2)./(pow+1.75*1e5)];

 

 % %%%%%%%%%%%%%%%%%%%%%

 

 pru=v_gaussmixp(vuvfea,m_u,v_u,w_u);  % Log probability of being unvoiced

 prv=v_gaussmixp(vuvfea,m_v,v_v,w_v);  % Log probability of being voiced

 

 pv=(1+exp(pru-prv)).^(-1);

 

 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 % Dynamic programming

 

 % w(1): relative amp, voiced local cost

 % w(2): median pitch deviation cost

 % w(3): df cost weight

 % w(4): max df cost

 % w(5): relative amp cost for missing peaks (very high)

 % w(6): df mean

 

 w = p.w;

 

 % Relative amplitude

 camp = -amp./repmat(max(amp,[],2),1,numopt);  % relative amplitude used as cost

 camp(amp==0)=w(5); % If no frequency found

 

 % Time interval for the median frequency

 tmf = p.tmf; % in sec

 inmf = round(tmf/txinc);

 

 %--------------------------------------------------------------------------

 % FORWARDS

 % Initialize values

 cost = zeros(nframes,numopt);

 prev = zeros(nframes,numopt);

 medfx = zeros(nframes,1);

 dffact=2/txinc;

 

 % First time frame

 % cost(1,:) = w(1)*ramp(1,:);

 cost(1,:) = w(1)*camp(1,:);  % only one cost term for first frame

 fpos = ff(1:min(inmf,end),1);

 mf=median(fpos(pv(1:min(inmf,end))>0.6));   % calculate median frequency of first 2 seconds

 if isnan(mf)

     mf=median(fpos(pv(1:min(inmf,end))>0.5));

     if isnan(mf)

         mf=median(fpos(pv(1:min(inmf,end))>0.4));

         if isnan(mf)

             mf=median(fpos(pv(1:min(inmf,end))>0.3)); % ==== clumsy way of ensuring that we take the best frames ====

             if isnan(mf)

                 mf=0;

             end

         end

     end

 end

 medfx(1)=mf;

 

 for i=2:nframes              % main dynamic programming loop

     if i>inmf

         fpos = ff(i-inmf:i,1);  % fpos is the highest peak in each frame

         mf=median(fpos(pv(1:inmf)>0.6));  % find median frequency over past 2 seconds

         if isnan(mf)

             mf=median(fpos(pv(1:inmf)>0.5));

             if isnan(mf)

                 mf=median(fpos(pv(1:inmf)>0.4));

                 if isnan(mf)

                     mf=median(fpos(pv(1:inmf)>0.3));% ==== clumsy way of ensuring that we take the best frames ====

                     if isnan(mf)

                         mf=0;

                     end

                 end

             end

         end

     end

     medfx(i)=mf;

     % Frequency difference between candidates and cost

     df = dffact*(repmat(ff(i,:).',1,numopt) - repmat(ff(i-1,:),numopt,1))./(repmat(ff(i,:).',1,numopt) + repmat(ff(i-1,:),numopt,1));

     costdf=w(3)*min((df-w(6)).^2,w(4));

 

     % Cost related to the median pitch

     if mf==0                                   % this test was inverted in the original version

         costf = zeros(1,numopt);

     else

         costf = abs(ff(i,:) - mf)./mf;

     end

     [cost(i,:),prev(i,:)]=min(costdf + repmat(cost(i-1,:),numopt,1),[],2); % ==== should we allow the possibility of skipping frames ? ====

     cost(i,:)=cost(i,:)+w(2)*costf + w(1)*camp(i,:);  % add on costs that are independent of previous path

 

 end

 

 % Traceback

 

 fx=zeros(nframes,1);

 ax=zeros(nframes,1);

 best = zeros(nframes,1);

 

 nose=find(cost(end,:)==min(cost(end,:))); % ==== bad method (dangerous) ===

 best(end)=nose(1);

 fx(end)=ff(end,best(end));

 ax(end)=amp(end,best(end));

 for i=nframes:-1:2

     best(i-1)=prev(i,best(i));

     fx(i-1)=ff(i-1,best(i-1));

     ax(i-1)=amp(i-1,best(i-1));

 end

 

 if nargout>=4

     fv.vuvfea=vuvfea;  % voiced-unvoiced features

     fv.best=best;  % selected path

     fv.ff=ff;  % pitch candidates

     fv.amp=amp;  % pitch candidate amplitudes

     fv.medfx=medfx;  % median pitch

     fv.w=w;  % DP weights

     fv.dffact=dffact;  % df scale factor

     fv.hist = [log(mean(O,2)) sum(amp,2)./((mean(O,2)))];

 end

 

 if ~nargout || any(m=='g') || any(m=='G')

     nax=0;  % number of axes sets to link

     msk=pv>0.5; % find voiced frames as a mask

     fxg=fx;

     fxg(~msk)=NaN; % allow only good frames

     fxb=fx;

     fxb(msk)=NaN; % allow only bad frames

     if any(m=='G') || ~nargout && ~any(m=='g')

         clf;

         v_spgrambw(s,fs,p.sopt); % draw spectrogram with log axes

         hold on

         plot(tx,log10(fxg),'-b',tx,log10(fxb),'-r'); % fx track

         yy=get(gca,'ylim');

         plot(tx,yy(1)+yy*[-1;1]*(0.02+0.05*pv),'-k'); % P(V) track

         hold off

         nax=nax+1;

         axh(nax)=gca;

         if any(m=='g')

             figure;   % need a new figure if plotting two graphs

         end

     end

     if any(m=='g')

         ns=length(s);

         [tsr,ix]=sort([(1:ns)/fs 0.5*(tx(1:end-1)+tx(2:end))']); % intermingle speech and frame boundaries

         jx(ix)=1:length(ix); % create inverse index

         sp2fr=jx(1:ns)-(0:ns-1);  % speech sample to frame number

         spmsk=msk(sp2fr);   % speech sample voiced mask

         sg=s;

         sg(~spmsk)=NaN;   % good speech samples only

         sb=s;

         sb(spmsk)=NaN;    % bad speech samples only

         clf;

         subplot(5,1,1);

         plot(tx,pv,'-b',(1:ns)/fs,0.5*mod(cumsum(fx(sp2fr)/fs),1)-0.6,'-b');

         nax=nax+1;

         axh(nax)=gca;

         ylabel('\phi(t), P(V)');

         set(gca,'ylim',[-0.65 1.05]);

         subplot(5,1,2:3);

         plot((1:ns)/fs,sg,'-b',(1:ns)/fs,sb,'-r');

         nax=nax+1;

         axh(nax)=gca;

         subplot(5,1,4:5);

         plot(tx,fxg,'-b',tx,fxb,'-r');

         ylabel('Pitch (Hz)');

         %         semilogy(tx,fxg,'-b',tx,fxb,'-r');

         %         ylabel(['Pitch (' v_yticksi 'Hz)']);

         set(gca,'ylim',[min(fxg)-30 max(fxg)+30]);

         nax=nax+1;

         axh(nax)=gca;

     end

     if nax>1

         linkaxes(axh,'x');

     end

 end

 

 function y=smooth(x,n)

 nx=size(x,2);

 nf=size(x,1);

 c=cumsum(x,2);

 y=[c(:,1:2:n)./repmat(1:2:n,nf,1) (c(:,n+1:end)-c(:,1:end-n))/n (repmat(c(:,end),1,floor(n/2))-c(:,end-n+2:2:end-1))./repmat(n-2:-2:1,nf,1)];

 

 function y=timesm(x,n)

 if ~mod(n,2)

     n = n+1;

 end

 nx=size(x,2);

 nf=size(x,1);

 c=cumsum(x,1);

 mid = round(n/2);

 y=[c(mid:n,:)./repmat((mid:n).',1,nx); ...

     (c(n+1:end,:)-c(1:end-n,:))/n; ...

     (repmat(c(end,:),mid-1,1) - c(end-n+1:end-mid,:))./repmat((n-1:-1:mid).',1,nx)];

}

// run the fft on points and return the per-bucket results
vector<double> fft(vector<double> &points, int buckets) {
    // how to interpret: https://www.gaussianwaves.com/2015/11/interpreting-fft-results-complex-dft-frequency-bins-and-fftshift/
    // we're going to return just the positive power, square rooted, which I think should approximate matlab's...
    const int numFft = buckets*2;
    const int numSam = numFft;
    static float samp[numSam] = {0};
    static kiss_fft_cpx kout[numFft]={0,0}; // only numFft/2+1 are valid to us, rest are beyond nyquist
    static kiss_fftr_cfg cfg	= NULL;

    // convert to float samples for ease of use
    int len = points.size();
    if (len > numSam) len = numSam;
    if (len&1) --len;               // must be even
    if (len == 0) return false;     // no sample as default, tone

    // copy as much of the sample into our work buffer
    // this probably isn't needed in this implementation...
    for (int i=0; i<len; ++i) {
        samp[i] = (float)pSample[i];
    }

    // zero the rest of the buffer (duplicating is much worse)
    for (int idx=len; idx<numSam; ++idx) {
        samp[idx]=0;
    }

    if (NULL == cfg) {
        cfg = kiss_fftr_alloc(numFft, 0, NULL, NULL);
    }
    kiss_fftr(cfg, samp, kout);

    // create the return vector
    vector<double> ret(buckets, 0);

    for (int idx=0; idx<buckets; ++idx) {
        double mag2=kout[idx].r*kout[idx].r + kout[idx].i*kout[idx].i;
        if ((idx != 0) && (idx != numFft/2)) {
            mag2*=2;    // symmetric counterpart implied, except for 0 (DC) and len/2 (Nyquist)
        }               // whatever that means ;)
        ret[idx] = sqrt(mag2);
    }

    return ret;
}

// convert one WAV file
// szIn - input filename
// szOut - output filename
// Nvoices - number of voices to output (0 for default (3))
// returns true on success or false on failure
bool matlabConvert(const char *szIn, const char *szOut, int Nvoices) {
    vector<double> input;           // input = [];
                                    // close all
                                    // path = 'wavs\';
    vector<string> names(1,szIn);   // names = dir([path '*.wav']);
    size_t nfiles = names.size();   // nfiles = size(names,1);

    if (Nvoices == 0) {             // if isempty(input) 
        Nvoices = 3;                //   Nvoices = 3;
    }                               // elseif ischar(input)
                                    //   input = str2double(input);
                                    //   Nvoices = input;
                                    // end

    if (verbose) printf("Num. channels = %d\n", Nvoices);   // display(['Num. channels = ' int2str(Nvoices)])

    double Tntsc = 1.0/60.0;        // Tntsc = 1/60;
    int FFS = 8000;                 // FFS = 8000;

    // not /really/ a loop anymore, we could remove this
    for (string name:names) {       // for ii = 1:nfiles 
                                    // sprintf('file#%d  %s\n',ii-1,names(ii).name)
                                    // name = [ path names(ii).name];

        vector<double> X = wavread(name, FFS);  // [Y,FS,NBITS] = wavread(name);
        // Note: Linear resampling, original    // if size(Y,2)>1  % stereo to mono
        // used an FIR antialising lowpass      //     X = Y(:,1)+Y(:,2);
        // filter which we don't reproduce      // else
        // here.                                //     X = Y;
        // we read straight to 'X' because      // end
        // we don't need the temporary 'Y'      // [P, Q] = rat(FFS/FS);
                                                // X = resample(X,P,Q);

        int FS = FFS;                           // FS = FFS;
        // already added in wavread()           // X = [zeros(round(2*Tntsc*FS),1) ;X]; % prepend 2 frames of silence at start

        // we know it's positive, so floor() is fine
        double Nntsc = floor(Tntsc*FS);         // Nntsc = fix(Tntsc*FS);

        // No, not doing gfx                    // figure(1);

        // fairly complex pitch tracker function
        vector<double> fx;
        vector<double> tt;
        vector<double> pv;
        SFV fv;
        fxpefac(X, FS, Tntsc, fx, tt, pv, fv);  // [fx,tt,pv,fv] = fxpefac(X,FS,Tntsc,'g');

        // do something with the results - this all happens only once per file
        int Nblk = fx.size();                       // Nblk = length(fx);
        vector<double> Y((2+Nblk)*Nntsc,0);         // Y = zeros((2+Nblk)*Nntsc,1);
        vector<double> XX((2+Nblk)*Nntsc,0);        // XX= zeros((2+Nblk)*Nntsc,1);
        vector<vector<double>> f(Nblk,vector<double>(Nvoices,0)); // f = zeros(Nblk,Nvoices);
        vector<vector<double>> a(Nblk,vector<double>(Nvoices,0)); // a = zeros(Nblk,Nvoices);
        vector<double> FX = fx;                     // FX = fx;
        vector<double> PV = pv;                     // PV = pv;
        int Ndft = 2^16;

        // note: index changed to zero based - offset use ONLY
        for (int i=0; i<Nblk; ++i) {            // for i=1:Nblk
            double d = round(tt[i]-Tntsc/2)*FS);    // d = round((tt(i)-Tntsc/2)*FS);
            
            vector<double> tti;
            for (int n=0; n<floor((d+Nntsc-1)-d), ++n) {
                tti.push_back(d+n);                 // tti = d:(d+Nntsc-1);
            }
            // don't need this temporary 'x'        // x = X(tti);
            
            vector<double> XF = fft(X,Ndft);        // XF  =   abs(fft(x,Ndft));
                                                    // % phi = angle(fft(x,Ndft));
            double Fmin = max(110.0,FX[i]-1/Tntsc/2); // Fmin = max(110,FX(i)-1/Tntsc/2); % 110Hz -> about 1023 as period in SN76489

**************************************
        [pks,locs]= findpeaks(XF(1:round(Ndft/2)),'SORTSTR','descend'); 

        j = find(locs> Fmin/FS*Ndft);
        pks  = pks(j);
        locs = locs(j);
        
        if size(locs,1)<Nvoices
            ti = (round(Ndft/2)-(Nvoices-1):round(Ndft/2))';
            ti(1:length(locs)) = locs;
            locs = ti;
        end

        locs = locs(1:Nvoices);     % choose the Nvoices strongest frequencies
%         locs = sort(locs);

%         pks = XF(locs);
%         plot(FS*locs/Ndft,pks,'k*') 
        
%         l = round(FX(i)/FS*Ndft);
%         plot(FX(i),XF(l+1),'hb');
        
        y = zeros(size(x));

        freq = (locs-1)/Ndft*FS;
        amp  = XF(locs)/Ndft*1024;

        t = tti'/FS;

        for  j=1:Nvoices
% (tursi - not sure this sign() is supposed to be here. If it sounds bad, try without)    
            y = y + amp(j)*sign(sin(2*pi*freq(j)*t));

        end

        XX((i+1)*Nntsc+1:(i+2)*Nntsc) =  x;
        Y ((i+1)*Nntsc+1:(i+2)*Nntsc) =  y;
                
        f(i,:) = freq;
        a(i,:) = amp;
    end

    figure(1)
    subplot(5,1,1);
    plot((1:size(XX,1))/FS,XX,'r',(1:size(Y,1))/FS,Y,'b')

    sound(XX,FS)
    sound(Y,FS)
    [SNR,glo] = snrseg(Y,XX,FS,'Vq',Tntsc);
    disp(SNR)
    
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % write output
    % Coleco PSG SN76489
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    TP = uint16(3579545./(32*f));

    m = max(a(:));
    a = a/m;
    
    nSN = zeros(size(a));

    for i=1:Nblk
        for j=1:Nvoices
            
            k = round(-20*log10 (a(i,j))/30*15);
            if (k>=15) 
                k=15;
            end
            nSN(i,j) = k;
        end
    end
    
    fid = fopen([name '_sn76489.bin'],'wb');
    for i = 1:Nblk
        for j = 1:Nvoices
            k = round(TP(i,j)+1024*nSN(i,j));
            fwrite(fid,k,'uint16');
        end
    end
    fwrite(fid,-1,'integer*2');    
    fclose(fid);
    
%     fid = fopen([name '_sn76489TI.bin'],'wb');
%     for i = 1:Nblk
%         for j = 1:Nvoices
%             k = round(TP(i,j)+1024*nSN(i,j));
%             fwrite(fid,floor(k/256),'uchar');
%             fwrite(fid,bitand(k,255),'uchar');            
%         end
%     end
%     fwrite(fid,-1,'integer*2');    
%     fclose(fid);
        
end


/*************************************************
// Original credits and GPL reference for v_fxpefac

0022 % References
0023 %  [1]  S. Gonzalez and M. Brookes. PEFAC - a pitch estimation algorithm robust to high levels of noise.
0024 %       IEEE Trans. Audio, Speech, Language Processing, 22 (2): 518-530, Feb. 2014.
0025 %       doi: 10.1109/TASLP.2013.2295918.
0026 %  [2]  S.Gonzalez and M. Brookes,
0027 %       A pitch estimation filter robust to high levels of noise (PEFAC), Proc EUSIPCO,Aug 2011.
0028 
0029 % Bugs/Suggestions
0030 % (1) do long files in chunks
0031 % (2) option of n-best DP
0032 
0033 %       Copyright (C) Sira Gonzalez and Mike Brookes 2011
0034 %      Version: $Id: v_fxpefac.m 10865 2018-09-21 17:22:45Z dmb $
0035 %
0036 %   VOICEBOX is a MATLAB toolbox for speech processing.
0037 %   Home page: http://www.ee.ic.ac.uk/hp/staff/dmb/voicebox/voicebox.html
0038 %
0039 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
0040 %   This program is free software; you can redistribute it and/or modify
0041 %   it under the terms of the GNU General Public License as published by
0042 %   the Free Software Foundation; either version 2 of the License, or
0043 %   (at your option) any later version.
0044 %
0045 %   This program is distributed in the hope that it will be useful,
0046 %   but WITHOUT ANY WARRANTY; without even the implied warranty of
0047 %   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
0048 %   GNU General Public License for more details.
0049 %
0050 %   You can obtain a copy of the GNU General Public License from
0051 %   http://www.gnu.org/copyleft/gpl.html or by writing to
0052 %   Free Software Foundation, Inc.,675 Mass Ave, Cambridge, MA 02139, USA.
0053 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

*********************************/
