#include <SDL2/sdl.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>

struct osc
{
	float freq;
	float phase;
};



osc carrier {
	.freq = 440,
	.phase = 0
};

static float sampTime = 1.0f/48000;

static void audio_fill_buffer(void* userdata, Uint8* stream, int len)
{
	int16_t* buff = (int16_t*)stream;
	int samples = len / sizeof(int16_t);
	for(int i = 0; i < samples;++i)
	{
		float sample = sinf(carrier.phase) * 16000;
		carrier.phase += sampTime*carrier.freq;
		*buff++ = (int16_t)sample;
	}
}


int main(int argc, char*argv[])
{
	if( 0 != SDL_Init(SDL_INIT_AUDIO) )
	{
		printf("Error\n");
	}

	SDL_AudioSpec want,got;

	memset(&want,0,sizeof(want));

	want.freq = 48000;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 4096;
	want.callback = audio_fill_buffer;

	auto audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if( audio_device == 0 )
	{
		printf("open audio error %s\n", SDL_GetError() );
		return 1;
	}


	printf("Got an audio device %d with %d channels, %d frequency and %d samples\n", audio_device, got.channels, got.freq, got.samples);


	SDL_PauseAudioDevice(audio_device,0);
	SDL_Delay(2000);

	SDL_CloseAudio();
	SDL_Quit();
	return 0;
}