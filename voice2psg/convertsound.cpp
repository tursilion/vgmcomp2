// Based on Artrag's voice encoder, ported from MATLAB to C by Tursi

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// volume ratios (we'll divide by 32767.0 below)
double Vlms[] = { 32767, 26028, 20675, 16422, 13045, 10362,  8231,  6568,  5193,  4125,  3277,  2603,  2067,  1642,  1304,     0 };

// surprising, I thought WAV would be all encapsulated by now. But I guess not!
// return memory is allocated and must be freed by the user
// also fills in 'chans', 'freq', 'bits', and 'samples'
unsigned char *readwav(char *fn, int &chans, int &freq, int &bits, int &samples) {
	unsigned char buf[44];
	unsigned char *ret;

	// not intended to be all-encompassing, just intended to work
	FILE *fp = fopen(fn, "rb");
	if (NULL == fp) {
		printf("* can't open %s (code %d)\n", fn, errno);
		return NULL;
	}

	if (1 != fread(buf, 44, 1, fp)) {
		fclose(fp);
		printf("* can't read WAV header on %s\n", fn);
		return NULL;
	}

	if (0 != memcmp(buf, "RIFF", 4)) {
		fclose(fp);
		printf("* not RIFF on %s\n", fn);
		return NULL;
	}

	if (0 != memcmp(&buf[8], "WAVE", 4)) {
		fclose(fp);
		printf("* not WAVE on %s\n", fn);
		return NULL;
	}

	if (0 != memcmp(&buf[12], "fmt ", 4)) {
		// technically this could be elsewhere, but that'd be unusual
		fclose(fp);
		printf("* fmt not as expected on %s\n", fn);
		return NULL;
	}

	// okay, we know that we have at least got a header
	if (0 != memcmp(&buf[20], "\x1\x0", 2)) {
		fclose(fp);
		printf("* not PCM on %s\n", fn);
		return NULL;
	}

	chans = buf[22] + buf[23]*256;
	if ((chans < 1) || (chans > 2)) {
		fclose(fp);
		printf("* %d chans on %s (support 1-2)\n", chans, fn);
		return NULL;
	}

	freq = buf[24] + buf[25]*256 + buf[26]*65536 + buf[27]*16777216;
	if ((freq < 1000) || (freq > 224000)) {
		fclose(fp);
		printf("Freq of %d on %s (support 1000-224000)\n", freq, fn);
		return NULL;
	}

	bits = buf[34] + buf[35]*256;
	if ((bits != 8)&&(bits != 16)) {
		fclose(fp);
		printf("Bits are %d on %s (support 8,16)\n", bits, fn);
		return NULL;
	}

	if (0 != memcmp(&buf[36], "data", 4)) {
		// technically this could be elsewhere, but that'd be unusual
		fclose(fp);
		printf("* data not as expected on %s\n", fn);
		return NULL;
	}

	samples = buf[40] + buf[41]*256 + buf[42]*65536 + buf[43]*16777216;	// data size in bytes
	
	// use that value to get our buffer before we scale it down to actual samples
	ret = (unsigned char*)malloc(samples);

	// and read it - this works if the above did since we are stopped
	// right at the beginning of the data chunk
	if (fread(ret, 1, samples, fp) != samples) {
		printf("+ warning: failed to read all samples from %s\n", fn);
	}
	fclose(fp);

	// now convert bytes to samples
	samples /= (bits/8);	// by bytes per sample
	samples /= chans;		// by number of channels - NOW it's samples

	// and we're done
	return ret;
}

// the meat of the function - this is the main loop adapted from MATlab
void parse(int nVoices, char *inPath) {
	int Freq = 44100;
    int samplesPerFrame = Freq/60;  // exactly 735
	unsigned char *wavData, *p;
	double *tmpsamples, *samples;
	int chans, freq, bits, cnt;
	double maxval;
	int offset;

	printf("Num. channels = %d\n", nVoices);
	printf("Processing %s...\n", inPath);
	
	// read the WAVE file, get the data (Y), frequency (FS), and bits per sample (NBITS)
	wavData = readwav(inPath, chans, freq, bits, cnt);
	
	// convert integer samples to double samples
	// also converts stereo to mono
	tmpsamples = (double*)malloc(cnt*sizeof(double));
	p = wavData;
	switch (bits) {
		case 8:
			maxval = 128;
			offset = 128;
			break;
		case 16:
			maxval = 32768;
			offset = 32768;
			break;
	}
	for (int idx=0; idx<cnt; idx++) {
		int sample = *(p++);
		if (bits>8) sample+=(*(p++))*256;

		tmpsamples[idx] = (sample-offset)/maxval;

        if (chans > 2) {
            printf("Can't process WAVE with more than 2 channels\n");
            return;
        }

		if (chans > 1) {
			// only 2 channels supported
			int sample = *(p++);
			if (bits>8) sample+=(*(p++))*256;

			tmpsamples[idx] += (sample-offset)/maxval;
		}
	}

	// all done with the original data
	free(wavData);

	{
		// now resample - our current rate is freq, but we want FFS
		double stepSrc = FFS/(double)freq;
		double srcPos = 0;
		int newCnt = (int)(cnt * (FFS/(double)freq) + 0.5) + 1;		// new sample count
		samples = (double*)malloc(newCnt * sizeof(double));
		for (int idx=0; idx<newCnt; idx++) {
			samples[idx] = 0.0;
		}
		for (int idx = 0; idx<newCnt; idx++) {
			// this is not perfect, but it'll do
			samples[idx] = (samples[idx] + tmpsamples[(int)(srcPos+0.5)]) / 2.0;
			srcPos += stepSrc;
			if ((int)(srcPos+0.5) >= cnt) srcPos -= 1.0;	// clamp to the end, just in case
		}
		free(tmpsamples);
		cnt = newCnt;
		freq = FFS;
	}


---->	X = [zeros(round(2*Tntsc*freq),1) ;X];	// TODO: (2*(1/60)*8000) x X (2d array) zeros
    
	double Nntsc = floor(Tntsc * freq);		// number of NTSC frames per second ~133 at 8khz

---->	[fx,tt,pv,fv] = fxpefac(X,FS,Tntsc,'g');// TODO: coming back to this - this is the big one. Outputs list of 
											// frequency (hz), time (s), probability of voice, 'feature vector' structure
											// one for each frame provided

	Nblk = length(fx);                      // number of blocks/frames returned (just counts the frequency objects)
  
	Y = zeros((2+Nblk)*Nntsc,1);            // make a bunch of zeros (2D, but second dimension is 1)
	XX= zeros((2+Nblk)*Nntsc,1);            // make a bunch of zeros (2D, but second dimension is 1)
    
	f = zeros(Nblk,Nvoices);                // 2d array of zeros Nblk x Nvoices
	a = zeros(Nblk,Nvoices);                // 2d array of zeros Nblk x Nvoices

	FX = fx;                                // frequencies
	PV = pv;                                // probability of voice

	Ndft = 2^16;                            // 65536

	for i=1:Nblk                            // for each block
        
		d = round((tt(i)-Tntsc/2)*FS);      // time delta
        
		tti = d:(d+Nntsc-1);                // 2d array index: delta, NTSC offset
        
		x = X(tti);                         // initial zeros array
        
		XF  =   abs(fft(x,Ndft));           // fft this slice, 64k buckets

		Fmin = max(110,FX(i)-1/Tntsc/2);    // bottom clip to 110 hz for SN   % 110Hz -> about 1000 as period in SN76489
        
		[pks,locs]= findpeaks(XF(1:round(Ndft/2)),'SORTSTR','descend'); // find and sort peaks in descending order

		j = find(locs> Fmin/FS*Ndft);       // find only peaks larger than Fmin
		pks  = pks(j);
		locs = locs(j);

        
        
		if size(locs,1)<Nvoices             // if we don't have enough to fill all voices...
			ti = (round(Ndft/2)-(Nvoices-1):round(Ndft/2))';    // duplicate them, I think...
			ti(1:length(locs)) = locs;
			locs = ti;
		end

		locs = locs(1:Nvoices);             //% choose the Nvoices strongest frequencies
        
		y = zeros(size(x));
		freq = zeros(1,Nvoices);
		amp  = zeros(1,Nvoices);

		freq = (locs-1)/Ndft*FS;            // get the freq
		amp  = XF(locs)/Ndft*1024;          // get the amplitude

		t = tti'/FS;                        // time related

		for  j=1:Nvoices
            // seems to calculate the volume based on the expected position in the
            // sinewave.. this would be more accurate but does it matter when
            // we are sampling at just 60Hz?
			y = y + amp(j)*(sin(2*pi*freq(j)*t));   // amplitude * sinewave(frequency * time)
                                                    
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
	% MSX PSG - only frequency data
	%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	TP = uint16(3579545./(32*f));

	m = max(a(:));
	a = a/m;
    
	nSN = zeros(size(a));

	for i=1:Nblk
		for j=1:Nvoices
			[m,k] = min(abs(a(i,j)-Vlms));
			nSN(i,j) = k-1;
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
    
	fid = fopen([name '_sn76489TI.bin'],'wb');
	for i = 1:Nblk
		for j = 1:Nvoices
			k = round(TP(i,j)+1024*nSN(i,j));
			fwrite(fid,fix(k/256),'uchar');		// bug: fix() is rounding instead of truncating
			fwrite(fid,bitand(k,255),'uchar');            
		end
	end
	fwrite(fid,-1,'integer*2');    
	fclose(fid);
        
	if (!FindNextFile(&hFind, &findDat)) break;
}

int main(int argc, char *argv[]) {
	char path[1024];
	int Nvoices;

	// fix the volume ratios
	for (int idx=0; idx<sizeof(Vlms)/sizeof(double); idx++) {
		Vlms[idx] /= 32767.0;
	}

	// TODO: parse arguments for number voices and path
	strcpy(path, "wavs\\");
	Nvoices = 3;

	parse(Nvoices, path);

	return 0;
}
