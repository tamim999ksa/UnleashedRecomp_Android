#include <stdafx.h>

#include <bit>

#include "audio.h"
#include <kernel/memory.h>

#define AUDIO_DRIVER_KEY (uint32_t)('DAUD')

// Use to dump raw audio captures to the game folder.
//#define AUDIO_DUMP_SAMPLES_PATH "audio.pcm"

#ifdef AUDIO_DUMP_SAMPLES_PATH
std::ofstream g_audioDumpStream;
#endif

uint32_t XAudioRegisterRenderDriverClient(be<uint32_t>* callback, be<uint32_t>* driver)
{
#ifdef AUDIO_DUMP_SAMPLES_PATH
    g_audioDumpStream.open(AUDIO_DUMP_SAMPLES_PATH, std::ios::binary);
#endif

    *driver = AUDIO_DRIVER_KEY;
    XAudioRegisterClient(g_memory.FindFunction(*callback), callback[1]);
    return 0;
}

uint32_t XAudioUnregisterRenderDriverClient(uint32_t driver)
{
    return 0;
}

uint32_t XAudioSubmitRenderDriverFrame(uint32_t driver, void* samples)
{
#ifdef AUDIO_DUMP_SAMPLES_PATH
    static uint32_t xaudioSamplesBuffer[XAUDIO_NUM_SAMPLES * XAUDIO_NUM_CHANNELS];
    for (size_t i = 0; i < XAUDIO_NUM_SAMPLES; i++)
    {
        for (size_t j = 0; j < XAUDIO_NUM_CHANNELS; j++)
        {
            xaudioSamplesBuffer[i * XAUDIO_NUM_CHANNELS + j] = ByteSwap(((uint32_t *)samples)[j * XAUDIO_NUM_SAMPLES + i]);
        }
    }

    g_audioDumpStream.write((const char *)(xaudioSamplesBuffer), sizeof(xaudioSamplesBuffer));
#endif

    XAudioSubmitFrame(samples);
    return 0;
}

// XMA Hijack for OGG support
static std::unordered_map<uint32_t, Mix_Chunk*> g_xmaContexts;
static std::mutex g_xmaMutex;

uint32_t XMACreateContext(uint32_t dwSize, uint32_t pContextDataPtr, uint32_t pContextPtr)
{
    uint32_t inputBufferAddr = 0;
    uint32_t inputBufferSize = 0;

    void* contextDataHost = g_memory.Translate(pContextDataPtr);
    if (contextDataHost)
    {
        inputBufferAddr = ByteSwap(*(uint32_t*)contextDataHost);
        inputBufferSize = ByteSwap(*((uint32_t*)contextDataHost + 1));
    }

    if (inputBufferAddr)
    {
        void* bufferHost = g_memory.Translate(inputBufferAddr);
        if (bufferHost)
        {
             if (memcmp(bufferHost, "OggS", 4) == 0)
             {
                 SDL_RWops* rw = SDL_RWFromConstMem(bufferHost, inputBufferSize);
                 Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1);

                 if (chunk)
                 {
                     std::lock_guard<std::mutex> lock(g_xmaMutex);
                     static uint32_t s_handleCounter = 0x1000;
                     uint32_t handle = ++s_handleCounter;

                     g_xmaContexts[handle] = chunk;

                     if (pContextPtr)
                        *(be<uint32_t>*)g_memory.Translate(pContextPtr) = handle;

                     // Play immediately as fallback since we lack EnableContext hook
                     Mix_PlayChannel(-1, chunk, 0);

                     return 0;
                 }
             }
        }
    }

    // Fallback: Return a dummy handle so game doesn't crash
    if (pContextPtr)
        *(be<uint32_t>*)g_memory.Translate(pContextPtr) = 0xDEADBEEF;

    return 0;
}

uint32_t XMAReleaseContext(uint32_t pContext)
{
     std::lock_guard<std::mutex> lock(g_xmaMutex);
     auto it = g_xmaContexts.find(pContext);
     if (it != g_xmaContexts.end())
     {
         Mix_FreeChunk(it->second);
         g_xmaContexts.erase(it);
     }
     return 0;
}
