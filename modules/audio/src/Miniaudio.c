/* miniaudio's implementation, compiled once as C without the strict warnings (cmake/AffrescoUIAudio.cmake): the
   devices (CoreAudio, WASAPI, PulseAudio, ALSA, the null backend), the WAV, FLAC and MP3 decoders and the data
   converter. The options it is built with are the module's compile definitions, shared with its C++ files. */
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
