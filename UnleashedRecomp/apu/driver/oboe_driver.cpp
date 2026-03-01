#include <apu/audio.h>
#include <cpu/guest_thread.h>
#include <kernel/heap.h>
#include <os/logger.h>
#include <user/config.h>
#include <oboe/Oboe.h>
#include <mutex>
#include <atomic>
#include <concurrentqueue.h>
#include <sys/resource.h>

static PPCFunc* g_clientCallback{};
static uint32_t g_clientCallbackParam{}; // pointer in guest memory

class OboeAudioDriver : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    struct AudioFrame {
        float samples[XAUDIO_NUM_CHANNELS * XAUDIO_NUM_SAMPLES];
        size_t count;
    };

    oboe::ManagedStream stream;
    moodycamel::ConcurrentQueue<AudioFrame> audioQueue;
    std::atomic<bool> downMixToStereo{true};
    int32_t framesPerBurst = 0;

    OboeAudioDriver() : audioQueue(128) {}

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        size_t channels = oboeStream->getChannelCount();
        float *output = static_cast<float *>(audioData);
        size_t totalSamples = numFrames * channels;

        AudioFrame frame;
        if (!audioQueue.try_dequeue(frame)) {
            memset(output, 0, totalSamples * sizeof(float));
            return oboe::DataCallbackResult::Continue;
        }

        size_t samplesToCopy = std::min(totalSamples, frame.count);
        memcpy(output, frame.samples, samplesToCopy * sizeof(float));

        if (samplesToCopy < totalSamples) {
            memset(output + samplesToCopy, 0, (totalSamples - samplesToCopy) * sizeof(float));
        }

        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override {
        LOGFN_INFO("Oboe Audio Stream error: {}", oboe::convertToText(error));
    }
};

static std::unique_ptr<OboeAudioDriver> g_oboeDriver;
static std::unique_ptr<std::thread> g_audioThread;
static std::atomic<bool> g_audioThreadShouldExit{false};

void XAudioInitializeSystem() {
    g_oboeDriver = std::make_unique<OboeAudioDriver>();

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Stereo)
           ->setSampleRate(XAUDIO_SAMPLES_HZ)
           ->setUsage(oboe::Usage::Game)
           ->setContentType(oboe::ContentType::Game)
           ->setDataCallback(g_oboeDriver.get())
           ->setErrorCallback(g_oboeDriver.get());

    oboe::Result result = builder.openStream(g_oboeDriver->stream);
    if (result != oboe::Result::OK) {
        LOGFN_ERROR("Failed to open Oboe stream: {}", oboe::convertToText(result));
        return;
    }

    g_oboeDriver->framesPerBurst = g_oboeDriver->stream->getFramesPerBurst();
    LOGFN_INFO("Oboe Stream burst size: {}", g_oboeDriver->framesPerBurst);

    g_oboeDriver->stream->requestStart();
}

static void AudioThread() {
    GuestThreadContext ctx(0);

    // Set high priority for the audio generation thread
    setpriority(PRIO_PROCESS, 0, -15);

    while (!g_audioThreadShouldExit) {
        // Optimal queue size for low latency vs stability
        size_t targetQueueSize = std::max<size_t>(2, (g_oboeDriver->framesPerBurst + XAUDIO_NUM_SAMPLES - 1) / XAUDIO_NUM_SAMPLES);

        if (g_oboeDriver && g_oboeDriver->audioQueue.size_approx() < targetQueueSize) {
            if (g_clientCallback) {
                ctx.ppcContext.r3.u32 = g_clientCallbackParam;
                g_clientCallback(ctx.ppcContext, g_memory.base);
            }
        } else {
            // High-precision sleep
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
}

void XAudioRegisterClient(PPCFunc* callback, uint32_t param) {
    auto* pClientParam = static_cast<uint32_t*>(g_userHeap.Alloc(sizeof(param)));
    uint32_t swappedParam = param;
    ByteSwapInplace(swappedParam);
    *pClientParam = swappedParam;
    g_clientCallbackParam = g_memory.MapVirtual(pClientParam);
    g_clientCallback = callback;

    g_audioThreadShouldExit = false;
    g_audioThread = std::make_unique<std::thread>(AudioThread);
}

void XAudioSubmitFrame(void* samples) {
    if (!g_oboeDriver) return;

    auto floatSamples = reinterpret_cast<be<float>*>(samples);
    float masterVol = Config::MasterVolume;

    OboeAudioDriver::AudioFrame frame;
    if (g_oboeDriver->downMixToStereo) {
        frame.count = 2 * XAUDIO_NUM_SAMPLES;
        for (size_t i = 0; i < XAUDIO_NUM_SAMPLES; i++) {
            float ch0 = floatSamples[0 * XAUDIO_NUM_SAMPLES + i];
            float ch1 = floatSamples[1 * XAUDIO_NUM_SAMPLES + i];
            float ch2 = floatSamples[2 * XAUDIO_NUM_SAMPLES + i];
            float ch4 = floatSamples[4 * XAUDIO_NUM_SAMPLES + i];
            float ch5 = floatSamples[5 * XAUDIO_NUM_SAMPLES + i];

            // Standard downmix: L = L + 0.707*C + Ls, R = R + 0.707*C + Rs
            frame.samples[i * 2 + 0] = (ch0 + ch2 * 0.707f + ch4) * masterVol;
            frame.samples[i * 2 + 1] = (ch1 + ch2 * 0.707f + ch5) * masterVol;
        }
    } else {
        frame.count = XAUDIO_NUM_CHANNELS * XAUDIO_NUM_SAMPLES;
        for (size_t i = 0; i < XAUDIO_NUM_SAMPLES; i++) {
            for (size_t j = 0; j < XAUDIO_NUM_CHANNELS; j++) {
                frame.samples[i * XAUDIO_NUM_CHANNELS + j] = floatSamples[j * XAUDIO_NUM_SAMPLES + i] * masterVol;
            }
        }
    }
    g_oboeDriver->audioQueue.enqueue(frame);
}
