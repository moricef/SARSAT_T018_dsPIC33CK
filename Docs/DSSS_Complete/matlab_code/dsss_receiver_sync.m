function [syncedData, status] = dsss_receiver_sync(rxBuffer, system, settings)
% DSSS_RECEIVER_SYNC - Complete receiver synchronization chain
% Based on MATLAB DSSS receiver examples for COSPAS-SARSAT
%
% Input:
%   rxBuffer - Received sample buffer with noise and impairments
%   system   - System parameters structure
%   settings - Receiver settings structure
%
% Output:
%   syncedData - Synchronized and corrected signal
%   status     - Synchronization status structure

% Initialize status structure
status = struct();
status.preambleFound = false;
status.freqOffset = 0;
status.timingLocked = false;
status.snrEstimate = 0;

% Step 1: Automatic Gain Control (AGC)
[agcOutput, agcGain] = automaticGainControl(rxBuffer, system);
status.agcGain = agcGain;

% Step 2: Preamble Detection with frequency search
[preambleIdx, freqOffset] = preambleDetection(agcOutput, system, settings);
if isempty(preambleIdx)
    fprintf('Preamble not detected\n');
    syncedData = [];
    return;
end

status.preambleFound = true;
status.preambleIndex = preambleIdx;
fprintf('Preamble found at index: %d\n', preambleIdx);

% Extract burst from buffer
burstLength = system.packetLength * system.spreadingFactor * settings.sps;
if preambleIdx + burstLength > length(agcOutput)
    fprintf('Incomplete burst in buffer\n');
    syncedData = [];
    return;
end

rxBurst = agcOutput(preambleIdx:preambleIdx + burstLength - 1);

% Step 3: Coarse Frequency Correction
[coarseSync, coarseEst] = coarseFrequencyCorrection(rxBurst, system, settings);
status.coarseFreqOffset = coarseEst;
fprintf('Coarse frequency offset: %.3f kHz\n', coarseEst/1000);

% Step 4: Fine Frequency and Phase Correction
[fineSync, phaseError] = fineFrequencyCorrection(coarseSync, system, settings);
status.fineFreqOffset = mean(diff(phaseError)) * settings.fs / (2*pi);
fprintf('Fine frequency offset: %.3f Hz\n', status.fineFreqOffset);

% Step 5: Timing Recovery
[timingSync, timingError] = timingRecovery(fineSync, system, settings);
status.timingLocked = std(timingError(end-100:end)) < 0.1;
fprintf('Timing recovery: %s\n', status.timingLocked ? 'Locked' : 'Unlocked');

% Step 6: Phase Ambiguity Resolution
[resolvedData, phaseRotation] = phaseAmbiguityResolution(timingSync, system);
status.phaseRotation = phaseRotation;
fprintf('Phase rotation resolved: %d degrees\n', phaseRotation * 90);

syncedData = resolvedData;
status.snrEstimate = estimateSNR(syncedData, system);
fprintf('Estimated SNR: %.1f dB\n', status.snrEstimate);

end

function [agcOut, gain] = automaticGainControl(rxIn, system)
% Automatic Gain Control implementation

% AGC parameters
adaptRate = 0.01;
avgLength = 10 * 8;  % 10 symbols * 8 samples/symbol
maxGain = 60;  % dB

% Initialize AGC
gain = 1;
agcOut = zeros(size(rxIn));
powerEst = 0;

for i = 1:length(rxIn)
    % Update power estimate
    powerEst = (1 - adaptRate) * powerEst + adaptRate * abs(rxIn(i))^2;
    
    % Update gain to maintain unit power
    if powerEst > 0
        targetGain = 1 / sqrt(powerEst);
        gain = min(targetGain, 10^(maxGain/20));
    end
    
    % Apply gain
    agcOut(i) = gain * rxIn(i);
    
    % Saturate to prevent overflow
    if abs(agcOut(i)) > 1.2
        agcOut(i) = 1.2 * sign(agcOut(i));
    end
end

end

function [startIdx, freqOffset] = preambleDetection(rxSamples, system, settings)
% Preamble detection with frequency offset search

% Create preamble reference
preambleLength = 175;  % Shortened for better frequency tolerance
preambleOffset = 200;  % Skip AGC settling time
preamble = createPreambleReference(system, preambleLength, preambleOffset);

% Convert OQPSK to QPSK for symbol-based detection
rxQPSK = oqpskToQpsk(rxSamples, settings.sps);

% Search over frequency offsets
maxDoppler = settings.maxDoppler;
freqStep = 150;  % Hz
bestCorr = 0;
startIdx = [];
freqOffset = 0;

for fOffset = -maxDoppler:freqStep:maxDoppler
    % Apply frequency offset to reference
    preambleShifted = applyFrequencyOffset(preamble, system.chipRate, fOffset);
    
    % Correlate with received signal (first half of buffer only)
    searchLength = min(length(rxQPSK), length(rxSamples)/2);
    [corrPeak, corrIdx] = correlateSignals(rxQPSK(1:searchLength), ...
                                          preambleShifted, settings.sps);
    
    if corrPeak > bestCorr
        bestCorr = corrPeak;
        startIdx = corrIdx;
        freqOffset = fOffset;
    end
    
    if bestCorr > 0.4  % Good enough correlation found
        break;
    end
end

if bestCorr < 0.3
    startIdx = [];  % Preamble not found
end

end

function rxQPSK = oqpskToQpsk(rxOQPSK, sps)
% Convert OQPSK to QPSK by removing Q channel delay

realPart = real(rxOQPSK(1:end-sps/2));
imagPart = imag(rxOQPSK(sps/2+1:end));
rxQPSK = complex(realPart, imagPart);

end

function preambleRef = createPreambleReference(system, length, offset)
% Create preamble reference sequence

% Generate PRN sequences
DSSS = generatePRNSequences();

% Extract preamble portion
startIdx = offset + 1;
endIdx = offset + length;
prnI = DSSS.PRN_I(startIdx:endIdx);
prnQ = DSSS.PRN_Q(startIdx:endIdx);

% Create QPSK preamble (alternating 0,1 pattern spread)
preambleBits = repmat([0 1], 1, length/2);
preambleRef = [];

for i = 1:length(preambleBits)
    if preambleBits(i) == 0
        symbol = complex(2*prnI(i)-1, 2*prnQ(i)-1);
    else
        symbol = complex(2*(~prnI(i))-1, 2*(~prnQ(i))-1);
    end
    preambleRef = [preambleRef symbol];
end

end

function shiftedSig = applyFrequencyOffset(signal, sampleRate, freqOffset)
% Apply frequency offset to signal

t = (0:length(signal)-1) / sampleRate;
phaseOffset = exp(1j * 2 * pi * freqOffset * t);
shiftedSig = signal .* phaseOffset;

end

function [corrPeak, peakIdx] = correlateSignals(rxSig, refSig, sps)
% Correlate received signal with reference

% Polyphase correlation for better timing resolution
numPhases = sps;
maxCorr = 0;
peakIdx = 1;

for phase = 0:numPhases-1
    % Downsample with different phase offsets
    rxDownsampled = rxSig(phase+1:sps:end);
    
    if length(rxDownsampled) >= length(refSig)
        % Cross-correlate
        corr = abs(xcorr(rxDownsampled(1:length(refSig)), refSig));
        [corrMax, idx] = max(corr);
        
        if corrMax > maxCorr
            maxCorr = corrMax;
            peakIdx = (idx-1) * sps + phase + 1;
        end
    end
end

corrPeak = maxCorr / length(refSig);  % Normalize

end

function [correctedSig, freqEst] = coarseFrequencyCorrection(rxBurst, system, settings)
% Coarse frequency offset estimation and correction

% Use built-in frequency offset estimator approach
% Estimate frequency offset over the burst
N = length(rxBurst);
freqResolution = 1;  % Hz
freqRange = 10000;   % ±10 kHz search range

% Simple frequency estimation using FFT-based method
fftSize = 2^nextpow2(N);
freqs = (-fftSize/2:fftSize/2-1) * settings.fs / fftSize;

% Find peak in frequency domain
rxFFT = fftshift(fft(rxBurst, fftSize));
[~, peakIdx] = max(abs(rxFFT));
freqEst = freqs(peakIdx);

% Limit frequency estimate to reasonable range
freqEst = max(-freqRange, min(freqRange, freqEst));

% Apply frequency correction
t = (0:N-1) / settings.fs;
correction = exp(-1j * 2 * pi * freqEst * t);
correctedSig = rxBurst .* correction;

end

function [syncedSig, phaseErr] = fineFrequencyCorrection(rxBurst, system, settings)
% Fine frequency and phase synchronization

% Carrier synchronizer parameters
loopBW = 0.01;
dampingFactor = 0.707;
sps = settings.sps;

% Initialize phase-locked loop
phaseErr = zeros(1, length(rxBurst));
syncedSig = zeros(size(rxBurst));
phaseEst = 0;
freqEst = 0;

% PLL gains
K1 = 4 * dampingFactor * loopBW / (1 + 2*dampingFactor*loopBW + loopBW^2);
K2 = 4 * loopBW^2 / (1 + 2*dampingFactor*loopBW + loopBW^2);

for i = 1:length(rxBurst)
    % Apply current phase estimate
    syncedSig(i) = rxBurst(i) * exp(-1j * phaseEst);
    
    % Phase error detection (for QPSK)
    if mod(i, sps) == 0  % Symbol sampling points
        symbol = syncedSig(i);
        % Simple phase error detector
        phaseErr(i) = imag(symbol * conj(sign(symbol)));
    else
        phaseErr(i) = phaseErr(max(1,i-1));
    end
    
    % Update PLL
    freqEst = freqEst + K2 * phaseErr(i);
    phaseEst = phaseEst + freqEst + K1 * phaseErr(i);
    
    % Wrap phase
    phaseEst = mod(phaseEst + pi, 2*pi) - pi;
end

end

function [recoveredSig, timingErr] = timingRecovery(rxBurst, system, settings)
% Symbol timing recovery for OQPSK

% Timing recovery parameters
loopBW = 0.001;
dampingFactor = 2;
detectorGain = 4;
sps = settings.sps;

% Initialize timing loop
timingErr = zeros(1, length(rxBurst));
recoveredSig = [];
sampleIdx = 1;
timingPhase = 0;

% Loop filter gains
K1 = 4 * dampingFactor * loopBW;
K2 = 4 * loopBW^2;

while sampleIdx + sps < length(rxBurst)
    % Extract symbol samples
    startIdx = round(sampleIdx);
    endIdx = min(startIdx + sps - 1, length(rxBurst));
    
    if endIdx > startIdx
        symbolSamples = rxBurst(startIdx:endIdx);
        
        % Interpolate to get symbol value
        if length(symbolSamples) == sps
            symbol = mean(symbolSamples);  % Simple averaging
            recoveredSig = [recoveredSig symbol];
            
            % Timing error detection (Gardner algorithm)
            if length(recoveredSig) >= 3
                early = recoveredSig(end-2);
                prompt = recoveredSig(end-1);
                late = recoveredSig(end);
                
                % Gardner timing error detector
                timingError = real((late - early) * conj(prompt));
                timingErr(length(recoveredSig)) = timingError;
                
                % Update timing with loop filter
                timingPhase = timingPhase + K1 * timingError;
                sampleIdx = sampleIdx + sps + K2 * timingError + timingPhase;
            else
                sampleIdx = sampleIdx + sps;
            end
        else
            break;
        end
    else
        break;
    end
end

end

function [resolvedSig, phaseRot] = phaseAmbiguityResolution(rxSymbols, system)
% Resolve 90-degree phase ambiguities in QPSK

% Test all possible phase rotations and I/Q swaps
testRotations = [0, pi/2, pi, 3*pi/2];
testSwaps = [false, true];  % I/Q swap

% Create reference preamble pattern
preambleRef = createPreamblePattern(system);
bestCorr = 0;
phaseRot = 0;
resolvedSig = rxSymbols;

for rotIdx = 1:length(testRotations)
    for swapIdx = 1:length(testSwaps)
        % Apply phase rotation
        testSig = rxSymbols * exp(1j * testRotations(rotIdx));
        
        % Apply I/Q swap if needed
        if testSwaps(swapIdx)
            testSig = complex(imag(testSig), real(testSig));
        end
        
        % Demodulate and check preamble
        demodBits = qpskDemodulate(testSig(1:system.preambleLength));
        
        % Correlate with expected preamble
        preamblePattern = repmat([0 1], 1, system.preambleLength/2);
        correlation = sum(demodBits == preamblePattern) / system.preambleLength;
        
        if correlation > bestCorr
            bestCorr = correlation;
            phaseRot = rotIdx - 1;  % 0, 1, 2, 3 for 0°, 90°, 180°, 270°
            resolvedSig = testSig;
        end
    end
end

fprintf('Phase ambiguity resolved with %.1f%% correlation\n', bestCorr * 100);

end

function bits = qpskDemodulate(symbols)
% Simple QPSK demodulation

bits = zeros(1, length(symbols));
for i = 1:length(symbols)
    if real(symbols(i)) >= 0 && imag(symbols(i)) >= 0
        bits(i) = 0;  % Q1: 00
    elseif real(symbols(i)) < 0 && imag(symbols(i)) >= 0
        bits(i) = 1;  % Q2: 01
    elseif real(symbols(i)) < 0 && imag(symbols(i)) < 0
        bits(i) = 2;  % Q3: 10
    else
        bits(i) = 3;  % Q4: 11
    end
end

% Convert to binary (simplified)
bits = mod(bits, 2);

end

function snr = estimateSNR(signal, system)
% Estimate SNR from synchronized signal

% Simple SNR estimation based on constellation variance
symbolPower = mean(abs(signal).^2);

% Estimate noise from constellation scatter
idealSymbols = sign(real(signal)) + 1j * sign(imag(signal));
noisePower = mean(abs(signal - idealSymbols).^2);

if noisePower > 0
    snr = 10 * log10(symbolPower / noisePower);
else
    snr = 60;  % Very high SNR
end

end

function DSSS = generatePRNSequences()
% Generate PRN sequences (placeholder - use actual LFSR)

% This is a simplified version - real implementation would use
% proper LFSR with polynomial x^23 + x^18 + 1
numChips = 256 * 300;  % Total chips needed
DSSS.PRN_I = randi([0 1], 1, numChips);
DSSS.PRN_Q = randi([0 1], 1, numChips);

end

function preamblePattern = createPreamblePattern(system)
% Create expected preamble bit pattern

preamblePattern = repmat([0 1], 1, system.preambleLength/2);

end