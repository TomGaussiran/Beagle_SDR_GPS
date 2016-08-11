// iq_stream.c - Stream raw IQ samples   (c) 2016 Johnathan York

// Based on iq_display extension Copyright (c) 2016 John Seamons, ZL/KF6VO



#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#ifndef EXT_IQ_STREAM
	void iq_stream_main() {}
#else

#include "types.h"
#include "kiwi.h"
#include "data_pump.h"
#include "gps.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define IQ_STREAM_DEBUG_MSG	true
#define IQ_STREAM_DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own iq_stream[] data structure.

struct iq_stream_t {
	int rx_chan;
	int run;
	float gain;
};

//#define	CUTESDR_SCALE	15			// +/- 1.0 -> +/- 32.0K (s16 equivalent)
#define MAX_VAL ((float) ((1 << CUTESDR_SCALE) - 1))

static iq_stream_t iq_stream[RX_CHANS];


#define	IQ_CLEAR		2
#define	IQ_SAMPLES	3


void iq_stream_data(int rx_chan, int nsamps, TYPECPX *samps)
{
	iq_stream_t *e = &iq_stream[rx_chan];
	int i;

  /* Send samples out */
	int16_t out_samples[nsamps*2];

  for (i=0; i<nsamps; i++) {
    float re = e->gain * (float) samps[i].re;
    float im = e->gain * (float) samps[i].im;

    if(re<-32768) re=-32768;
    if(re> 32767) re= 32767;
    if(im<-32768) im=-32768;
    if(im> 32767) im= 32767;

    out_samples[i*2+0] = re;
    out_samples[i*2+1] = im;
  }

	ext_send_data_msg(rx_chan, IQ_STREAM_DEBUG_MSG, IQ_SAMPLES, (u1_t*) out_samples, nsamps*2*sizeof(out_samples[0]));
}

bool iq_stream_msgs(char *msg, int rx_chan)
{
	iq_stream_t *e = &iq_stream[rx_chan];
	int n;
	
	printf("### iq_stream_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		e->gain = 1;
		ext_send_msg(e->rx_chan, IQ_STREAM_DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run) {
			ext_register_receive_iq_samps(iq_stream_data, rx_chan);
			ext_send_msg(rx_chan, IQ_STREAM_DEBUG_MSG, "SET sample_rate_hz=%lf", ext_get_sample_rateHz());
			ext_send_msg(rx_chan, IQ_STREAM_DEBUG_MSG, "SET center_freq_hz=%lf", ext_get_center_freqHz(rx_chan));
		} else {
			ext_unregister_receive_iq_samps(rx_chan);
	  }
		return true;
	}
	
	int gain;
	n = sscanf(msg, "SET gain=%d", &gain);
	if (n == 1) {
		// 0 .. +100 dB of MAX_VAL
		e->gain = pow(10.0, ((float) gain) / 20.0);
		printf("e->gain %.6f\n", e->gain);
		return true;
	}
	
	float offset;
	n = sscanf(msg, "SET offset=%f", &offset);
	if (n == 1) {
		adc_clock -= adc_clock_offset;
		adc_clock_offset = offset;
		adc_clock += adc_clock_offset;
		gps.adc_clk_corr++;
		printf("adc_clock %.6f offset %.2f\n", adc_clock/1e6, offset);
		return true;
	}
	
	n = strcmp(msg, "SET clear");
	if (n == 0) {
		//e->cma = e->ncma = 0;
		return true;
	}
	
	return false;
}

void iq_stream_close(int rx_chan)
{

}

void iq_stream_main();

ext_t iq_stream_ext = {
	"iq_stream",
	iq_stream_main,
	iq_stream_close,
	iq_stream_msgs,
};

void iq_stream_main()
{
    double frate = ext_get_sample_rateHz();
    printf("iq_stream_main audio sample rate = %.1f\n", frate);

	ext_register(&iq_stream_ext);
}

#endif
