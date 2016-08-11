#!/usr/bin/python
""" iq_plotspec.py - Plot a spectogram of saved IQ data """
from pylab import *
import h5py
import sys

filename=sys.argv[1]

f=h5py.File(filename,'r')
ds=f['Data']
d=ds[:]
specgram(d,Fs=ds.attrs['timeDenominator']/ds.attrs['cadence'],Fc=ds.attrs.get('centerFreqHz',0))
ylabel('Freq (Hz)')
xlabel('Time (s)')
title('Spectrogram\n{0}'.format(filename))
show()
