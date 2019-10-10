#!/usr/bin/env python3
import sys
sys.path.append('.')
import numpy as np
import liquid as dsp
import matplotlib.pyplot as plt

# generate a frame
fg      = dsp.fg64()
n       = fg.get_frame_length()
header  = np.zeros(( 8,), dtype=np.int8)
payload = np.zeros((64,), dtype=np.int8)
frame   = np.zeros(( n,), dtype=np.csingle)
fg.execute(header, payload, frame)

# receive frame (TODO: pass callback)
fs      = dsp.fs64()
fs.execute(frame)

# compute spectral response
nfft = 2400
psd = 20*np.log10(np.abs(np.fft.fftshift(np.fft.fft(frame, nfft))))

# plot frame and its spectrum
fig, ax = plt.subplots(2,figsize=(8,8))
t = np.arange(n)
ax[0].plot(t, np.real(frame), t, np.imag(frame))
ax[0].set_xlabel('Delay [samples]')
ax[0].set_ylabel('Frame Samples')
ax[0].grid(True, zorder=5)
ax[1].plot(np.arange(nfft)/nfft-0.5, psd)
ax[1].set_xlabel('Normalized Frequency [f/F_s]')
ax[1].set_ylabel('Power Spectral Density [dB]')
ax[1].grid(True, zorder=5)
plt.show()
