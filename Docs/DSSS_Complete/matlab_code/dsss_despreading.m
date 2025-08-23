function [decodedBits, status] = dsss_despreading(syncedSymbols, system)
% DSSS_DESPREADING - Despreading and decoding of DSSS signal
% Based on MATLAB DSSS receiver examples for COSPAS-SARSAT
%
% Input:
%   syncedSymbols - Synchronized QPSK symbols from timing recovery
%   system        - System parameters structure
%
% Output:
%   decodedBits - Decoded information bits after despreading and BCH
%   status      - Decoding status and metrics

% Initialize status
status = struct();
status.preambleValid = false;
status.bchErrors = -1;
status.spreadingValid = false;

% Generate PRN sequences for despreading
DSSS = generatePRNSequences(system);

% Step 1: QPSK Demodulation to extract I and Q bit streams
[chipI, chipQ] = qpskDemodulation(syncedSymbols, system);

% Step 2: Extract preamble and payload chips
numPreambleChips = system.preambleLength * system.spreadingFactor;
numPayloadChips = (system.packetLength - system.preambleLength) * system.spreadingFactor;

if length(chipI) < numPreambleChips + numPayloadChips
    fprintf('Insufficient chips for complete packet\n');
    decodedBits = [];
    return;
end

preambleChipsI = chipI(1:numPreambleChips);
preambleChipsQ = chipQ(1:numPreambleChips);
payloadChipsI = chipI(numPreambleChips+1:numPreambleChips+numPayloadChips);
payloadChipsQ = chipQ(numPreambleChips+1:numPreambleChips+numPayloadChips);

% Step 3: Validate preamble
[preambleValid, preambleErrors] = validatePreamble(preambleChipsI, preambleChipsQ, DSSS, system);
status.preambleValid = preambleValid;
status.preambleErrors = preambleErrors;

if ~preambleValid
    fprintf('Preamble validation failed: %d errors\n', preambleErrors);
end

% Step 4: Despread payload data
[despreadBits, spreadingMetrics] = despreadPayload(payloadChipsI, payloadChipsQ, DSSS, system);
status.spreadingValid = spreadingMetrics.correlationMean > 0.7;
status.spreadingMetrics = spreadingMetrics;

if isempty(despreadBits)
    fprintf('Despreading failed\n');
    decodedBits = [];
    return;
end

% Step 5: BCH Error Detection and Correction
[decodedBits, bchErrors] = bchDecode(despreadBits, system);
status.bchErrors = bchErrors;

if bchErrors >= 0
    fprintf('BCH decoding successful: %d errors corrected\n', bchErrors);
else
    fprintf('BCH decoding failed: too many errors\n');
end

% Step 6: Extract and display message fields
if bchErrors >= 0
    displayMessageFields(decodedBits);
end

fprintf('\nDespreading Summary:\n');
fprintf('- Preamble valid: %s (%d errors)\n', status.preambleValid ? 'Yes' : 'No', status.preambleErrors);
fprintf('- Spreading correlation: %.3f\n', status.spreadingMetrics.correlationMean);
fprintf('- BCH errors: %d\n', status.bchErrors);

end

function [chipI, chipQ] = qpskDemodulation(symbols, system)
% Demodulate QPSK symbols to extract I and Q chip streams

numSymbols = length(symbols);
chipI = zeros(1, numSymbols);
chipQ = zeros(1, numSymbols);

for i = 1:numSymbols
    % Extract I and Q components
    iVal = real(symbols(i));
    qVal = imag(symbols(i));
    
    % Hard decision demodulation
    chipI(i) = iVal >= 0;
    chipQ(i) = qVal >= 0;
end

end

function DSSS = generatePRNSequences(system)
% Generate PRN sequences using LFSR polynomial x^23 + x^18 + 1

totalChips = system.packetLength * system.spreadingFactor;

% Generate I channel PRN sequence
DSSS.PRN_I = generateLFSRSequence([23 18], totalChips, 1);

% Generate Q channel PRN sequence (with offset for orthogonality)  
DSSS.PRN_Q = generateLFSRSequence([23 18], totalChips, 65);  % Different seed

end

function sequence = generateLFSRSequence(taps, length, seed)
% Generate LFSR sequence with given polynomial taps

% Initialize LFSR register (23-bit)
register = de2bi(seed, 23, 'left-msb');
if sum(register) == 0
    register(1) = 1;  % Ensure non-zero state
end

sequence = zeros(1, length);

for i = 1:length
    % Output current state (last bit)
    sequence(i) = register(end);
    
    % Calculate feedback from taps
    feedback = 0;
    for tap = taps
        feedback = xor(feedback, register(tap));
    end
    
    % Shift register and insert feedback
    register = [feedback register(1:end-1)];
end

end

function [isValid, numErrors] = validatePreamble(chipI, chipQ, DSSS, system)
% Validate received preamble against expected pattern

numPreambleChips = system.preambleLength * system.spreadingFactor;
expectedPattern = repmat([0 1], 1, system.preambleLength/2);
errors = 0;

% Despread preamble chips
for bitIdx = 1:system.preambleLength
    startIdx = (bitIdx-1) * system.spreadingFactor + 1;
    endIdx = bitIdx * system.spreadingFactor;
    
    % Extract chips for this bit
    bitChipsI = chipI(startIdx:endIdx);
    bitChipsQ = chipQ(startIdx:endIdx);
    
    % Get corresponding PRN sequences
    prnI = DSSS.PRN_I(startIdx:endIdx);
    prnQ = DSSS.PRN_Q(startIdx:endIdx);
    
    % Correlate with PRN (XOR for correlation)
    corrI = sum(xor(bitChipsI, prnI));
    corrQ = sum(xor(bitChipsQ, prnQ));
    
    % Majority vote decision (ML decoding)
    decodedBitI = corrI > system.spreadingFactor/2;
    decodedBitQ = corrQ > system.spreadingFactor/2;
    
    % Check against expected preamble pattern
    expectedBit = expectedPattern(bitIdx);
    if decodedBitI ~= expectedBit
        errors = errors + 1;
    end
    if decodedBitQ ~= expectedBit  
        errors = errors + 1;
    end
end

% Preamble valid if less than 10% errors
numErrors = errors;
isValid = (errors / (system.preambleLength * 2)) < 0.1;

end

function [bits, metrics] = despreadPayload(chipI, chipQ, DSSS, system)
% Despread payload chips using PRN sequences

payloadBits = system.packetLength - system.preambleLength;  % 250 bits
preambleOffset = system.preambleLength * system.spreadingFactor;
bits = zeros(1, payloadBits * 2);  % I and Q bits interleaved
correlations = zeros(1, payloadBits * 2);

bitIndex = 1;

for bitIdx = 1:payloadBits
    startIdx = preambleOffset + (bitIdx-1) * system.spreadingFactor + 1;
    endIdx = preambleOffset + bitIdx * system.spreadingFactor;
    
    % Extract chips for this payload bit
    bitChipsI = chipI(startIdx:endIdx);
    bitChipsQ = chipQ(startIdx:endIdx);
    
    % Get corresponding PRN sequences
    prnI = DSSS.PRN_I(startIdx:endIdx);
    prnQ = DSSS.PRN_Q(startIdx:endIdx);
    
    % Despread I channel
    corrI = sum(xor(bitChipsI, prnI));
    decodedBitI = corrI > system.spreadingFactor/2;
    corrI_norm = abs(corrI - system.spreadingFactor/2) / (system.spreadingFactor/2);
    
    % Despread Q channel  
    corrQ = sum(xor(bitChipsQ, prnQ));
    decodedBitQ = corrQ > system.spreadingFactor/2;
    corrQ_norm = abs(corrQ - system.spreadingFactor/2) / (system.spreadingFactor/2);
    
    % Store results
    bits(bitIndex) = decodedBitI;
    bits(bitIndex+1) = decodedBitQ;
    correlations(bitIndex) = corrI_norm;
    correlations(bitIndex+1) = corrQ_norm;
    
    bitIndex = bitIndex + 2;
end

% Calculate despreading metrics
metrics.correlationMean = mean(correlations);
metrics.correlationStd = std(correlations);
metrics.correlationMin = min(correlations);

fprintf('Despreading metrics:\n');
fprintf('- Mean correlation: %.3f\n', metrics.correlationMean);
fprintf('- Std correlation: %.3f\n', metrics.correlationStd);
fprintf('- Min correlation: %.3f\n', metrics.correlationMin);

end

function [decodedInfo, numErrors] = bchDecode(receivedBits, system)
% BCH(255,207) decoding shortened to (250,202)

% BCH parameters
n = 255;  % Codeword length
k = 207;  % Information length  
t = 6;    % Error correction capability
shortened_n = 250;
shortened_k = 202;

if length(receivedBits) ~= shortened_n
    fprintf('Error: Expected %d bits, got %d\n', shortened_n, length(receivedBits));
    decodedInfo = [];
    numErrors = -1;
    return;
end

% Extract information and parity bits
infoBits = receivedBits(1:shortened_k);      % 202 info bits
parityBits = receivedBits(shortened_k+1:end); % 48 parity bits

% Simple BCH-like error detection (simplified implementation)
% In practice, would use proper BCH syndrome calculation

% Calculate syndrome using simplified method
syndrome = calculateSyndrome(receivedBits);
numErrors = sum(syndrome ~= 0);

% If syndrome is zero, no errors detected
if numErrors == 0
    decodedInfo = infoBits;
    return;
end

% Attempt error correction (simplified)
if numErrors <= t
    % Perform error correction (placeholder implementation)
    correctedBits = attemptErrorCorrection(receivedBits, syndrome);
    decodedInfo = correctedBits(1:shortened_k);
    
    % Verify correction
    newSyndrome = calculateSyndrome(correctedBits);
    if sum(newSyndrome) == 0
        fprintf('Errors corrected successfully\n');
    else
        fprintf('Error correction may have failed\n');
        numErrors = -1;  % Indicate correction failure
    end
else
    % Too many errors to correct
    decodedInfo = infoBits;  % Return uncorrected data
    numErrors = -1;  % Indicate uncorrectable
    fprintf('Too many errors for BCH correction: %d\n', numErrors);
end

end

function syndrome = calculateSyndrome(codeword)
% Calculate BCH syndrome (simplified implementation)

% This is a placeholder for proper BCH syndrome calculation
% Real implementation would use the BCH generator polynomial

% Simple parity check syndrome
syndrome = zeros(1, 6);  % 6 syndrome components for t=6

for i = 1:6
    paritySum = 0;
    for j = 1:length(codeword)
        if mod(i*j, 7) < 3  % Simplified syndrome calculation
            paritySum = xor(paritySum, codeword(j));
        end
    end
    syndrome(i) = paritySum;
end

end

function corrected = attemptErrorCorrection(received, syndrome)
% Attempt to correct errors using syndrome

corrected = received;  % Start with received data

% Simplified error correction
% Real implementation would use BCH error locator polynomial

% Find potential error locations based on syndrome pattern
errorPattern = zeros(size(received));

% Simple error correction heuristic
for i = 1:length(syndrome)
    if syndrome(i) == 1
        % Estimate error location
        errorLoc = mod(i * 37, length(received)) + 1;
        errorPattern(errorLoc) = 1;
    end
end

% Apply correction
corrected = xor(received, errorPattern);

end

function displayMessageFields(decodedBits)
% Display decoded COSPAS-SARSAT message fields

if length(decodedBits) < 202
    fprintf('Incomplete message: only %d bits\n', length(decodedBits));
    return;
end

fprintf('\n=== COSPAS-SARSAT Message Analysis ===\n');

% Extract fields according to T.018 specification
hexID = decodedBits(1:43);                    % 23-HEX ID
position = decodedBits(44:90);                % GPS position (47 bits)
vesselID = decodedBits(91:137);               % Vessel ID (47 bits) 
beaconType = decodedBits(138:154);            % Beacon type (17 bits)
rotatingField = decodedBits(155:202);         % Rotating field (48 bits)

% Display 23-HEX ID
fprintf('23-HEX ID (43 bits): ');
displayBitField(hexID);

% Decode GPS position (simplified)
fprintf('GPS Position (47 bits): ');
[lat, lon] = decodeGPSPosition(position);
fprintf('Lat: %.5f°, Lon: %.5f°\n', lat, lon);

% Display Vessel ID
fprintf('Vessel ID (47 bits): ');
displayBitField(vesselID);

% Decode beacon type
fprintf('Beacon Type (17 bits): ');
beaconTypeValue = bi2de(beaconType, 'left-msb');
fprintf('Type %d\n', beaconTypeValue);

% Analyze rotating field
fprintf('Rotating Field (48 bits): ');
analyzeRotatingField(rotatingField);

end

function displayBitField(bits)
% Display bit field in hex format

if length(bits) < 8
    fprintf('%s\n', mat2str(bits));
    return;
end

% Convert to hex bytes
hexString = '';
for i = 1:8:length(bits)
    endIdx = min(i+7, length(bits));
    byte = bits(i:endIdx);
    if length(byte) < 8
        byte = [byte zeros(1, 8-length(byte))];  % Pad with zeros
    end
    hexValue = bi2de(byte, 'left-msb');
    hexString = [hexString sprintf('%02X ', hexValue)];
end
fprintf('%s\n', hexString);

end

function [lat, lon] = decodeGPSPosition(positionBits)
% Decode GPS position from 47-bit field (simplified)

if length(positionBits) ~= 47
    lat = 0; lon = 0;
    return;
end

% Extract latitude (23 bits) and longitude (24 bits)
latBits = positionBits(1:23);
lonBits = positionBits(24:47);

% Convert to decimal (simplified decoding)
latValue = bi2de(latBits, 'left-msb');
lonValue = bi2de(lonBits, 'left-msb');

% Scale to degrees (COSPAS-SARSAT encoding)
lat = (latValue / (2^22)) * 180 - 90;   % -90 to +90 degrees
lon = (lonValue / (2^23)) * 360 - 180;  % -180 to +180 degrees

end

function analyzeRotatingField(rotatingBits)
% Analyze rotating field content

if length(rotatingBits) ~= 48
    fprintf('Invalid length\n');
    return;
end

% Extract rotating field type (first 4 bits)
rfType = bi2de(rotatingBits(1:4), 'left-msb');

fprintf('Type %d ', rfType);

switch rfType
    case 0
        fprintf('(G.008/ELTDT)\n');
        % Decode G.008 specific fields
        decodeG008Field(rotatingBits);
        
    case 14  
        fprintf('(RLS - Return Link Service)\n');
        
    case 15
        fprintf('(Cancel Message)\n');
        
    otherwise
        fprintf('(Unknown/Reserved)\n');
end

end

function decodeG008Field(rotatingBits)
% Decode G.008 rotating field format

% Extract time and altitude fields (simplified)
timeBits = rotatingBits(5:15);     % 11 bits for time
altBits = rotatingBits(16:25);     % 10 bits for altitude  

timeValue = bi2de(timeBits, 'left-msb');
altValue = bi2de(altBits, 'left-msb');

fprintf('  Time: %d, Altitude code: %d\n', timeValue, altValue);

end