""" iq_save.py - Script to save out IQ data to a file """
import websocket
import time
import h5py
import numpy
import argparse
import fractions
import sys

parser = argparse.ArgumentParser(description='Save IQ data from a kiwi SDR')
parser.add_argument('--kiwinetaddr',metavar='IP',type=str,help='Network address of the kiwi (default: %default)',default='beaglebone')
parser.add_argument('-o','--output',metavar="FILENAME.MDH",type=str,help="Output MDH File", required=True)
parser.add_argument('--rxchan',metavar="INT", type=int,help="RX Channel to use", required=True)
parser.add_argument('-d','--duration',metavar="SECS",type=float,help="Duration of collect (default: %default; 0=until killed)",default=10.)
args = parser.parse_args()

ofile=h5py.File(args.output,'w')



ws = websocket.create_connection("ws://beaglebone.lan:8073/{0:.0f}/EXT".format(round(time.time())))

ws.send('SERVER DE CLIENT openwebrx.js EXT')
ws.send('SET ext_switch_to_client=iq_stream first_time=1 rx_chan={rx_chan:d}'.format(rx_chan=args.rxchan));
ws.send('SET gain=48');
ws.send('SET run=1');

ds=ofile.create_dataset('Data',dtype=numpy.complex64,shape=(0,),maxshape=(None,))


ds.attrs['CLASS']='MDH'
ds.attrs['SUBCLASS']='PREDETECTION_SAMPLES'
sampleRate=8250. # May be overwritten by data from receiver below

while (args.duration==0.) or len(ds)/float(sampleRate)<args.duration:
  r=ws.recv()

  if r.startswith('DAT \x03'):
    samplesInt=numpy.frombuffer(r[5:],dtype='2i2')


    samplesComplex=numpy.zeros(len(samplesInt),dtype=ds.dtype)
    samplesComplex.real=samplesInt[:,0]
    samplesComplex.imag=samplesInt[:,1]

    existingDSLen=len(ds)
    ds.resize(existingDSLen+len(samplesComplex),axis=0)
    ds[existingDSLen:existingDSLen+len(samplesComplex)]=samplesComplex
    sys.stdout.write("Collected {0:4.1f} seconds...\r".format(len(ds)/float(sampleRate)))
    sys.stdout.flush()
  elif r.startswith('SET sample_rate_hz='):
    sample_rate=float(r.split('=')[1])
    sample_rate_frac=fractions.Fraction.from_float(sample_rate)
    ds.attrs['timeDenominator']=sample_rate_frac.numerator
    ds.attrs['cadence']=sample_rate_frac.denominator
  elif r.startswith('SET center_freq_hz='):
    ds.attrs['centerFreqHz']=float(r.split('=')[1])
  else:
    pass
    #print "MSG: ", repr(r)

print



"""
ws.send('SERVER DE CLIENT openwebrx.js FFT\n')
ws.send('SET send_dB=1\n')
ws.send('SET zoom=0 start=0\n')
ws.send('SET maxdb=0 mindb=-100\n')
ws.send('SET slow=2\n')
"""




"""
	
Length
Time
SERVER DE CLIENT openwebrx.js AUD	33	
21:22:13.438
SET squelch=0	13	
21:22:13.438
SET autonotch=0	15	
21:22:13.438
SET genattn=4095	16	
21:22:13.438
SET gen=0.000 mix=-1	20	
21:22:13.438
SET mod=am low_cut=-4000 high_cut=4000 freq=1000	48	
21:22:13.438
SET agc=1 hang=0 thresh=-120 slope=0 decay=200 manGain=0

"""


"""
SERVER DE CLIENT openwebrx.js EXT	33	
21:23:39.400
Binary Frame (Opcode 2)	1751	
21:23:39.480
Binary Frame (Opcode 2)	19	
21:23:39.480
SET ext_switch_to_client=iq_display first_time=1 rx_chan=2	58	
21:23:39.481
SET gain=15	11	
21:23:39.482
SET clear	9	
21:23:39.483
SET points=1024	15	
21:23:39.484
SET clear	9	
21:23:39.484
SET run=1	9	
21:23:39.485
SET clear

"""


"""
s=socket.socket()

s.connect(('beaglebone.lan',8073))

s.send('GET /1469765716561/FFT HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n\r\n')



s.send('SERVER DE CLIENT openwebrx.js FFT\n')
s.send('SET send_dB=1\n')
s.send('SET zoom=0 start=0\n')
s.send('SET maxdb=0 mindb=-100\n')
s.send('SET slow=2\n')


s.send('SET keepalive=1\n')

while True:
  r=s.recv(4096)
  if r: print repr(r)
"""

