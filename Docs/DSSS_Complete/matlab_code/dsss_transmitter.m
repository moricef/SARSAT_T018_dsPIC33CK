function tx = dsss_transmitter_example()
% DSSS_TRANSMITTER_EXAMPLE - COSPAS-SARSAT 2G Transmitter Implementation
% Based on MATLAB DSSS example for COSPAS-SARSAT specification T.018
%
% This function demonstrates the complete DSSS transmitter chain:
% 1. Message generation (300 bits frame)
% 2. BCH encoding (255,207) shortened to (250,202) 
% 3. DSSS spreading with PRN sequences
% 4. OQPSK modulation
% 5. Pulse shaping and oversampling

% System parameters (T.018 compliant)
system.fc = 406.05;                    % Carrier frequency (MHz)
system.chipRate = 38400;               % Chip rate (chips/s)
system.symbolRate = 300;               % Symbol rate (symbols/s) 
system.spreadingFactor = 256;          % Spreading factor
system.preambleLength = 50;            % Preamble length (symbols)
system.packetLength = 300;             % Total packet length (bits)

% PRN sequence generation (LFSR: x^23 + x^18 + 1)
DSSS = generatePRNSequences();

% Generate test message (202 information bits + 48 BCH parity)
messageInfo = generateTestMessage();

% BCH encoding (255,207) shortened to (250,202)
encodedMessage = bchEncode(messageInfo);

% Add preamble (50 bits alternating pattern)
preamble = repmat([0 1], 1, 25);
fullMessage = [preamble encodedMessage];

% DSSS spreading
[spreadI, spreadQ] = dsssSpread(fullMessage, DSSS);

% OQPSK modulation 
tx.symbols = oqpskModulate(spreadI, spreadQ);
tx.chips = length(spreadI);

% Oversampling for transmission
sps = 8;  % Samples per symbol
tx.samples = oversample(tx.symbols, sps);

fprintf('DSSS Transmitter Summary:\n');
fprintf('- Carrier: %.2f MHz\n', system.fc);
fprintf('- Chip rate: %d chips/s\n', system.chipRate);
fprintf('- Symbol rate: %d symbols/s\n', system.symbolRate);
fprintf('- Spreading factor: %d\n', system.spreadingFactor);
fprintf('- Total chips: %d\n', tx.chips);
fprintf('- OQPSK symbols: %d\n', length(tx.symbols));
fprintf('- Oversampled length: %d\n', length(tx.samples));

end

function DSSS = generatePRNSequences()
% Generate I and Q PRN sequences using LFSR polynomial x^23 + x^18 + 1

% Initial state (non-zero)
initialState = [1 zeros(1, 22)];

% LFSR for I channel 
DSSS.PRN_I = generateLFSR(initialState, [23 18], 256*300);

% LFSR for Q channel (90° phase offset)
shiftedState = circshift(initialState, 64);  % Quarter period offset
DSSS.PRN_Q = generateLFSR(shiftedState, [23 18], 256*300);

end

function sequence = generateLFSR(initialState, taps, length)
% Generate LFSR sequence with given taps and length

register = initialState;
sequence = zeros(1, length);

for i = 1:length
    sequence(i) = register(end);
    
    % Calculate feedback
    feedback = 0;
    for tap = taps
        feedback = xor(feedback, register(tap));
    end
    
    % Shift register and insert feedback
    register = [feedback register(1:end-1)];
end

end

function message = generateTestMessage()
% Generate 202-bit test message for COSPAS-SARSAT
% Format: 23-HEX ID + Position + Vessel ID + Type + Rotating field

% 23-HEX ID (43 bits) - Test beacon ID
hexID = [1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1];

% GPS Position (47 bits) - Grenoble test coordinates
% Latitude: 45.1885°N, Longitude: 5.7245°E encoded
position = [0 1 0 1 1 0 1 0 0 1 1 0 1 0 1 1 0 0 1 0 1 1 0 1 0 1 0 1 1 0 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 1];

% Vessel ID (47 bits) - Test vessel identification
vesselID = [1 1 0 0 1 0 1 1 0 1 0 0 1 1 0 1 0 1 1 0 0 1 0 1 1 0 1 0 0 1 1 0 1 0 1 1 0 0 1 0 1 0 1 1 0 1 1];

% Beacon type + spare (17 bits)
beaconType = [0 1 0 1 0 0 1 0 1 1 0 0 1 0 1 0 1];

% Rotating field (48 bits) - G.008 ELTDT format
rotatingField = [0 0 0 0 1 1 0 1 0 1 0 0 1 1 0 1 0 1 1 0 0 1 0 1 1 0 1 0 0 1 1 0 1 0 1 1 0 0 1 0 1 0 1 1 0 1 1 0];

% Combine all fields (total: 202 bits)
message = [hexID position vesselID beaconType rotatingField];

end

function encoded = bchEncode(message)
% BCH(255,207) encoding shortened to (250,202)
% Generates 48 parity bits for 202 information bits

% BCH generator polynomial (simplified representation)
% In practice, would use proper BCH encoding algorithm
parity = zeros(1, 48);

% Simple parity calculation (placeholder for full BCH)
for i = 1:48
    parityBit = 0;
    for j = 1:202
        if mod(i*j, 7) < 3  % Simplified BCH-like calculation
            parityBit = xor(parityBit, message(j));
        end
    end
    parity(i) = parityBit;
end

encoded = [message parity];  % 250 total bits

end

function [spreadI, spreadQ] = dsssSpread(message, DSSS)
% Spread message bits using DSSS PRN sequences

numBits = length(message);
spreadI = zeros(1, numBits * 256);
spreadQ = zeros(1, numBits * 256);

for i = 1:numBits
    startIdx = (i-1) * 256 + 1;
    endIdx = i * 256;
    
    if message(i) == 0
        % Bit 0: use PRN sequence as-is
        spreadI(startIdx:endIdx) = DSSS.PRN_I(startIdx:endIdx);
        spreadQ(startIdx:endIdx) = DSSS.PRN_Q(startIdx:endIdx);
    else
        % Bit 1: invert PRN sequence
        spreadI(startIdx:endIdx) = ~DSSS.PRN_I(startIdx:endIdx);
        spreadQ(startIdx:endIdx) = ~DSSS.PRN_Q(startIdx:endIdx);
    end
end

end

function symbols = oqpskModulate(spreadI, spreadQ)
% OQPSK modulation with half-symbol Q delay

% Convert bits to bipolar
bitsI = 2*spreadI - 1;  % 0->-1, 1->+1
bitsQ = 2*spreadQ - 1;

% OQPSK: delay Q channel by half symbol
delayedQ = [0 bitsQ(1:end-1)];

% Create complex symbols
symbols = complex(bitsI, delayedQ);

end

function oversampled = oversample(symbols, sps)
% Oversample symbols with pulse shaping

% Simple rectangular pulse shaping (could use RRC filter)
oversampled = zeros(1, length(symbols) * sps);

for i = 1:length(symbols)
    startIdx = (i-1) * sps + 1;
    endIdx = i * sps;
    oversampled(startIdx:endIdx) = symbols(i);
end

end