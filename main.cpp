#include <SDL2/sdl.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl_gl3.h"
#include <GL/gl3w.h>
#include "vulkfm.h"

static float sampTime;

static void audio_fill_buffer_s16(void* userdata, Uint8* stream, int len)
{
	int16_t* buff = (int16_t*)stream;
	int samples = len / sizeof(int16_t);

	VulkFM* synth = (VulkFM*)userdata;


	for(int i = 0; i < samples;++i)
	{
		float sample = synth->evaluate();
		synth->update(sampTime);
		// Clip instead of overflow
		sample = sample<-1.f?-1.f:(sample>1.f?1.f:sample);
		*buff++ = (int16_t)(sample*(32768>>1));
	}
}

static SDL_Window* window = nullptr;

static void open_window()
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	window = SDL_CreateWindow("VulkFM", SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if( window == nullptr)
	{
		printf("Could not create window. %s\n", SDL_GetError());
		exit(1);
	}

	/*SDL_GLContext context =*/ SDL_GL_CreateContext(window);

	int major, minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	printf( "OpenGL version %d.%d\n",major,minor);

	gl3wInit();
}


int main(int /*argc*/, char* /*argv*/[])
{
	VulkFM vulkSynth;

	if( 0 != SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) )
	{
		printf("Error\n");
	}

	SDL_AudioSpec want,got;

	memset(&want,0,sizeof(want));

	want.freq = 48000;
	want.format = AUDIO_S16SYS;
	// want.format = AUDIO_F32SYS;
	want.channels = 1;
	want.samples = 800;
	want.userdata = &vulkSynth;
	want.callback = audio_fill_buffer_s16;

	auto audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if( audio_device == 0 )
	{
		printf("open audio error %s\n", SDL_GetError() );
		return 1;
	}


	printf("Got an audio device %d with %d channels, %d frequency and %d samples in %d format.\n", audio_device, got.channels, got.freq, got.samples, got.format);


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


	open_window();
	if( window == nullptr)
		exit(1);

	ImGui_ImplSdlGL3_Init(window);

	sampTime = 1.0f/got.freq;

	SDL_PauseAudioDevice(audio_device,0);

	SDL_Event event;
	bool quit = false;

	int octave = 4;

	while(!quit)
	{
		while(SDL_PollEvent(&event))
		{
			ImGui_ImplSdlGL3_ProcessEvent(&event);

			// Also handle events myself
			switch(event.type)
			{
			case SDL_KEYDOWN:
				if(event.key.repeat != 0)
					continue;
				if(event.key.keysym.sym == SDLK_ESCAPE)
					quit = true;
				else {
					auto key = event.key.keysym.scancode;
					for(size_t i = 0 ; i < sizeof(keymap); ++i)
					{
						if(key == keymap[i]) {
							int note = i + 12*octave;
							vulkSynth.trigger(note,0,127);
						}
					}
				}
				break;

			case SDL_KEYUP:
				if(event.key.repeat == 0) {
					auto key = event.key.keysym.scancode;
					for(size_t i = 0 ; i < sizeof(keymap); ++i)
					{
						if(key == keymap[i])
						{
							int note = i + 12*octave;
							vulkSynth.release(note,0,127);
						}
					}
				}
				break;
			case SDL_QUIT:
				quit = true;
				break;
			}
		}

		ImGui_ImplSdlGL3_NewFrame(window);

		glClearColor(0.5f,0.6f,0.5f,1);
		glClear(GL_COLOR_BUFFER_BIT);

		// Render UI / visualization

		{
			auto instrument = vulkSynth.getInstrument(0);

			ImGui::Begin("VulkFM");

			if(ImGui::TreeNodeEx("General",ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Playback frequency is %dHz", got.freq);
				ImGui::Text("There are %d active voices", vulkSynth.activeVoices());
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
				ImGui::TreePop();
			}

			if(ImGui::TreeNode("Operators"))
			{
				// for operator in instrument...
				std::string name = "Operator x";

				int operatorCount = instrument->algo_.operatorCount;				

				for( int i = 0; i< operatorCount;++i)
				{
					name[9] = '1' + i;

					auto &conf = instrument->opConf_[i];

					if(ImGui::TreeNode(name.c_str()))
					{
						// static float a = 0.5f, al= 1.0f, d= 0.5f, s = 0.5f,r = 0.5f;

						const char* waveforms_names[] { "Sine", "Square", "ClampSine", "AbsSine" };

			            ImGui::Combo("Waveform", (int*)&conf.oscWaveform, waveforms_names, IM_ARRAYSIZE(waveforms_names), 4);
						ImGui::SliderFloat("Attack", &conf.env.attack, 0.0f, 8.0f);
						ImGui::SliderFloat("Attack level", &conf.env.attackLevel, 0.0f, 1.0f);
						ImGui::SliderFloat("Decay", &conf.env.decay, 0.0f, 4.0f);
						ImGui::SliderFloat("Sustain", &conf.env.sustain, 0.0f, 1.0f);
						ImGui::SliderFloat("Release", &conf.env.release, 0.0f, 4.0f);

						ImGui::DragFloat("Freq scale", &conf.freqScale, 0.25f, 0.f, 14.0f);

						ImGui::TreePop();
					}
				}

				ImGui::TreePop();

			}
			ImGui::End();

			ImGui::SetNextWindowSize(ImVec2(528,332));
			ImGui::Begin("Wave");
			auto samples = vulkSynth.getOutBuffer();

		    ImGui::PlotLines("", samples, 512, 0,  NULL, -1.5f, 1.5f, ImVec2(512,300), sizeof(float));

			ImGui::End();

			// Show algorithm data
			ImGui::Begin("Instrument");

			ImGui::Text("outputs: %d %d %d %d %d %d"
				, instrument->algo_.outs[0]
				, instrument->algo_.outs[1]
				, instrument->algo_.outs[2]
				, instrument->algo_.outs[3]
				, instrument->algo_.outs[4]
				, instrument->algo_.outs[5]
			);

			ImGui::Text("modulation: %d %d %d %d %d %d"
				, instrument->algo_.mods[0]
				, instrument->algo_.mods[1]
				, instrument->algo_.mods[2]
				, instrument->algo_.mods[3]
				, instrument->algo_.mods[4]
				, instrument->algo_.mods[5]
			);

			ImGui::End();
		}

		ImGui::Render();

		SDL_GL_SwapWindow(window);
		fflush(stdout);
		SDL_Delay(10);
	}
	ImGui_ImplSdlGL3_Shutdown();
	printf("leaving\n");
	SDL_DestroyWindow(window);
	SDL_CloseAudio();
	SDL_Quit();
	return 0;
}