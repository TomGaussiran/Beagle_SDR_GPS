""" iq_save.py - Script to save out IQ data to a file """
import websocket
import time
import h5py
import numpy


ws = websocket.create_connection("ws://beaglebone.lan:8073/{0:.0f}/EXT".format(round(time.time())))

ws.send('SERVER DE CLIENT openwebrx.js EXT')
ws.send('SET ext_switch_to_client=iq_stream first_time=1 rx_chan=0');
ws.send('SET gain=48');
ws.send('SET run=1');


ofile=h5py.File('mydata.mdh','w')
ds=ofile.create_dataset('Data',dtype=numpy.complex64,shape=(0,),maxshape=(None,))

sampleRate=8250 # Must be an integer for now
ds.attrs['CLASS']='MDH'
ds.attrs['SUBCLASS']='PREDETECTION_SAMPLES'
ds.attrs['timeDenominator']=sampleRate
ds.attrs['cadence']=1
#ds.attrs['centerFreqHz']=<enter manually with hdfview for now

while True:
  r=ws.recv()

  if r.startswith('DAT \x03'):
    samplesInt=numpy.frombuffer(r[5:],dtype='2i2')


    samplesComplex=numpy.zeros(len(samplesInt),dtype=ds.dtype)
    samplesComplex.real=samplesInt[:,0]
    samplesComplex.imag=samplesInt[:,1]

    existingDSLen=len(ds)
    ds.resize(existingDSLen+len(samplesComplex),axis=0)
    ds[existingDSLen:existingDSLen+len(samplesComplex)]=samplesComplex
    print "Collected {0:4.1f} seconds...".format(len(ds)/float(sampleRate))





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
