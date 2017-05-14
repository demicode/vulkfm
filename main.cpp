#include <SDL2/sdl.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>

#include "vulkfm.h"



static float sampTime;

static Instrument inst0;

static void audio_fill_buffer(void* userdata, Uint8* stream, int len)
{
	static float time = 0;
	int16_t* buff = (int16_t*)stream;
	int samples = len / sizeof(int16_t);

	for(int i = 0; i < samples;++i)
	{
		float sample = inst0.evaluate();
		inst0.update(sampTime);
		*buff++ = (int16_t)(sample*32767);

		time += sampTime;
	}
}


static SDL_Window* open_window()
{
	SDL_Window* window = SDL_CreateWindow("VulkFM", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0 );
	if( window == NULL)
	{
		printf("Could not create window\n");
		exit(1);
	}
	return window;
}



static void test_envelope()
{
	Env e;
	e.trigger();
	for(int i = 0; i < 50; ++i)
	{
		auto val = e.evaluate();
		printf("%d: %f\n", i, val);
		e.update(0.05f);
	}
	e.release();
	for(int i = 0; i < 50; ++i)
	{
		auto val = e.evaluate();
		printf("%d: %f\n", i, val);
		e.update(0.05f);
	}

}



int main(int argc, char*argv[])
{

/*	test_envelope();
	return 0;
*/

	if( 0 != SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) )
	{
		printf("Error\n");
	}

	SDL_AudioSpec want,got;

	memset(&want,0,sizeof(want));

	want.freq = 48000;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 800;
	want.callback = audio_fill_buffer;

	auto audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if( audio_device == 0 )
	{
		printf("open audio error %s\n", SDL_GetError() );
		return 1;
	}


	printf("Got an audio device %d with %d channels, %d frequency and %d samples\n", audio_device, got.channels, got.freq, got.samples);


	int keymap[] {
		SDL_SCANCODE_Q,
		SDL_SCANCODE_2,
		SDL_SCANCODE_W,
		SDL_SCANCODE_3,
		SDL_SCANCODE_E,
		SDL_SCANCODE_R,
		SDL_SCANCODE_5,
		SDL_SCANCODE_T,
		SDL_SCANCODE_6,
		SDL_SCANCODE_Y,
		SDL_SCANCODE_7,
		SDL_SCANCODE_U,
		SDL_SCANCODE_I,
		SDL_SCANCODE_9,
		SDL_SCANCODE_O,
		SDL_SCANCODE_0,
		SDL_SCANCODE_P,
	};


	SDL_Window *window = open_window();

	sampTime = 1.0f/got.freq;



	//Setup instruments

	auto op = inst0.getOperator(1);
	op.env_ = Env( 0, 0, 1.0, 0 );


	SDL_PauseAudioDevice(audio_device,0);

	SDL_Event event;
	bool quit = false;
	while(!quit)
	{
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_KEYDOWN:
				if(event.key.repeat != 0)
					continue;
				if(event.key.keysym.sym == SDLK_ESCAPE)
					quit = true;
				else {
					auto key = event.key.keysym.scancode;
					int octave = 4;
					for(int i = 0 ; i < sizeof(keymap); ++i)
					{
						if(key == keymap[i])
						inst0.trigger(i + 12*octave);
					}
				}
				 if(event.key.keysym.sym == SDLK_SPACE)
				break;

			case SDL_KEYUP:
				if(event.key.repeat == 0)
				{
					auto key = event.key.keysym.scancode;
					for(int i = 0 ; i < sizeof(keymap); ++i)
					{
						if(key == keymap[i]) inst0.release();
					}
				}
				break;
			case SDL_QUIT:
				quit = true;
				break;
			}
		}
		fflush(stdout);
	}

	printf("leaving\n");
	SDL_DestroyWindow(window);
	SDL_CloseAudio();
	SDL_Quit();
	return 0;
}