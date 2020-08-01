
input = [];

close all

path = 'wavs\';

names = dir([path '*.wav']);
nfiles = size(names,1);

if isempty(input) 
    Nvoices = 3;
elseif ischar(input)
    input = str2double(input);
    Nvoices = input;
end


display(['Num. channels = ' int2str(Nvoices)])

Tntsc = 1/60;

FFS = 8000;

for ii = 1:nfiles 

    sprintf('file#%d  %s\n',ii-1,names(ii).name)
    name = [ path names(ii).name];

    [Y,FS,NBITS] = wavread(name);

    if size(Y,2)>1
        X = Y(:,1)+Y(:,2);
    else
        X = Y;
    end

    [P, Q] = rat(FFS/FS);
    
    X = resample(X,P,Q);

    FS = FFS;
    
    X = [zeros(round(2*Tntsc*FS),1) ;X];
    
    Nntsc = fix(Tntsc*FS);

    figure(1);
    [fx,tt,pv,fv] = fxpefac(X,FS,Tntsc,'g');

    Nblk = length(fx);
  
    Y = zeros((2+Nblk)*Nntsc,1);
    XX= zeros((2+Nblk)*Nntsc,1);
    
    f = zeros(Nblk,Nvoices);
    a = zeros(Nblk,Nvoices);

    FX = fx;
    PV = pv;


    Ndft = 2^16;

    for i=1:Nblk
        
        d = round((tt(i)-Tntsc/2)*FS);
        
        tti = d:(d+Nntsc-1);
        
        x = X(tti);
        
        XF  =   abs(fft(x,Ndft));
%         phi = angle(fft(x,Ndft));

        Fmin = max(110,FX(i)-1/Tntsc/2);                      % 110Hz -> about 1000 as period in SN76489
        
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


