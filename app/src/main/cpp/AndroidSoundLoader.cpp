#include <SDL.h>
#include <SDL_mixer.h>

#include <cstring>
#include <limits>

// SDL_mixer expands mono WAVs to stereo before resampling them. For the
// game's common 22.05 kHz mono effects, resample one channel instead and
// duplicate the result. Keep the mixer's 44.1 kHz stereo format for music.
Mix_Chunk* AndroidLoadWavChunk(const char* path) {
    int mixerFrequency = 0;
    int mixerChannels = 0;
    Uint16 mixerFormat = 0;
    if (!Mix_QuerySpec(&mixerFrequency, &mixerFormat, &mixerChannels) ||
        mixerFrequency != 44100 || mixerFormat != AUDIO_S16SYS || mixerChannels != 2) {
        return Mix_LoadWAV(path);
    }

    SDL_AudioSpec sourceSpec{};
    Uint8* sourceData = nullptr;
    Uint32 sourceLength = 0;
    if (!SDL_LoadWAV(path, &sourceSpec, &sourceData, &sourceLength)) {
        return Mix_LoadWAV(path);
    }
    if (sourceSpec.freq != 22050 || sourceSpec.format != AUDIO_S16SYS ||
        sourceSpec.channels != 1 || sourceLength == 0) {
        SDL_FreeWAV(sourceData);
        return Mix_LoadWAV(path);
    }

    SDL_AudioCVT conversion{};
    if (SDL_BuildAudioCVT(&conversion, sourceSpec.format, 1, sourceSpec.freq,
                          mixerFormat, 1, mixerFrequency) < 0 ||
        sourceLength > static_cast<Uint32>(std::numeric_limits<int>::max()) ||
        conversion.len_mult <= 0 ||
        sourceLength > static_cast<Uint32>(std::numeric_limits<int>::max() / conversion.len_mult)) {
        SDL_FreeWAV(sourceData);
        return Mix_LoadWAV(path);
    }

    conversion.len = static_cast<int>(sourceLength) & ~1;
    conversion.buf = static_cast<Uint8*>(SDL_calloc(1, static_cast<size_t>(conversion.len) * conversion.len_mult));
    if (!conversion.buf) {
        SDL_FreeWAV(sourceData);
        SDL_OutOfMemory();
        return nullptr;
    }
    std::memcpy(conversion.buf, sourceData, conversion.len);
    SDL_FreeWAV(sourceData);

    if (SDL_ConvertAudio(&conversion) < 0) {
        SDL_free(conversion.buf);
        return nullptr;
    }
    if (conversion.len_cvt < 0 ||
        static_cast<Uint32>(conversion.len_cvt) > std::numeric_limits<Uint32>::max() / 2) {
        SDL_free(conversion.buf);
        SDL_SetError("Converted WAV is too large");
        return nullptr;
    }

    const size_t stereoLength = static_cast<size_t>(conversion.len_cvt) * 2;
    auto* stereoData = static_cast<Sint16*>(SDL_malloc(stereoLength));
    auto* chunk = static_cast<Mix_Chunk*>(SDL_malloc(sizeof(Mix_Chunk)));
    if (!stereoData || !chunk) {
        SDL_free(stereoData);
        SDL_free(chunk);
        SDL_free(conversion.buf);
        SDL_OutOfMemory();
        return nullptr;
    }
    const auto* monoData = reinterpret_cast<const Sint16*>(conversion.buf);
    for (int i = 0; i < conversion.len_cvt / static_cast<int>(sizeof(Sint16)); ++i) {
        stereoData[2 * i] = monoData[i];
        stereoData[2 * i + 1] = monoData[i];
    }
    SDL_free(conversion.buf);

    chunk->allocated = 1;
    chunk->abuf = reinterpret_cast<Uint8*>(stereoData);
    chunk->alen = static_cast<Uint32>(stereoLength);
    chunk->volume = MIX_MAX_VOLUME;
    return chunk;
}
