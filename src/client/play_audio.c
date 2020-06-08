#include "uchat.h"

static void print_s_info(SF_INFO s_info) {
    printf ("frames = %lld\n", s_info.frames);
    printf ("samplerate = %d\n", s_info.samplerate);
    printf ("channels = %d\n", s_info.channels);
    printf ("format = %d\n", s_info.format);
    printf ("sections = %d\n", s_info.sections);
    printf ("seekable = %d\n", s_info.seekable);
    fflush(stdout);
}

static int exit_program(PaError err, const char *text, SNDFILE* a_file) {
    printf("error in %s =%s\n", text, Pa_GetErrorText(err));
    fflush(stdout);

    Pa_Terminate();
    if (a_file)
        sf_close(a_file);
    return 1;
}

static int init_stream(PaStream** stream, SF_INFO s_info) {
    PaStreamParameters output_parameters;
    PaError err;

    output_parameters.device = Pa_GetDefaultOutputDevice();
    output_parameters.channelCount = s_info.channels;
    output_parameters.sampleFormat = paFloat32;
    output_parameters.suggestedLatency = Pa_GetDeviceInfo(output_parameters.device)->defaultLowOutputLatency;
    output_parameters.hostApiSpecificStreamInfo = NULL;
    err = Pa_OpenStream(stream,
                        NULL,
                        &output_parameters,
                        s_info.samplerate,
                        FRAMES_PER_BUFFER,
                        paClipOff,
                        NULL,
                        NULL);

    if (err != paNoError || ! stream)
        return exit_program(err, "Pa_OpenStream", NULL);
    return Pa_StartStream(*stream);
}

int mx_play_sound_file(char *file_name, char *start_time, char *duration_t) {
    PaStream* stream = NULL;
    SNDFILE* a_file = NULL;
    SF_INFO s_info;
    PaError err;
    sf_count_t length;	// length of file in frames
    sf_count_t start_point; // start_point of frames read
    sf_count_t end_point; // end point of frames playing

    double starttime = 0;
    double duration = 0;
    sf_count_t read_count = 0;
    sf_count_t read_sum = 0;

    if ((err = Pa_Initialize()) != paNoError)
        return exit_program(err, "Pa_Initialize", a_file);

    memset(&s_info, 0, sizeof(s_info));

    if (!(a_file = sf_open(file_name, SFM_READ, &s_info))) {
        printf("error sf_info =%d\n", sf_error(a_file));
        fflush(stdout);
        Pa_Terminate();
        return 1;
    }

    print_s_info(s_info);
    printf("play 2\n");
    fflush(stdout);
    length = s_info.frames;
    if (start_time) {
        starttime = atof(start_time);
        start_point = (sf_count_t) starttime * s_info.samplerate;
    }
    else {
        start_point = 0;
    }  // start play with the beginning

    if (duration_t) {  // время проигрывания
        duration = atof(duration_t);
        end_point = (sf_count_t) (duration * s_info.samplerate + start_point);
        end_point = (end_point < length) ? end_point : length;
    }
    else {
        end_point = length;
        duration = (double) (end_point - start_point) / (double) s_info.samplerate;
    }

    printf("length frames =%lld\n", length); fflush(stdout);
    printf("starttime  =%f\n", starttime); fflush(stdout);
    printf("duration  =%f\n", duration); fflush(stdout);
    printf("start_point frames =%lld\n", start_point); fflush(stdout);
    printf("end_point frames =%lld\n\n", end_point); fflush(stdout);

    err = init_stream(&stream, s_info);
    if (err != paNoError)
        return exit_program(err, "init_stream", a_file);

    float data[FRAMES_PER_BUFFER * s_info.channels];
    memset(data, 0, sizeof(data));

    int subFormat = s_info.format & SF_FORMAT_SUBMASK;
    double scale = 1.0;
    int m = 0;
    sf_seek(a_file, start_point, SEEK_SET);
    while ((read_count = sf_read_float(a_file, data, FRAMES_PER_BUFFER ))) {
        if (subFormat == SF_FORMAT_FLOAT || subFormat == SF_FORMAT_DOUBLE) {
            for (m = 0 ; m < read_count ; ++m) {
                data[m] *= scale;
            }
        }
        read_sum += read_count;
//        if (read_sum > end_point - start_point) {
//            printf("read_sum frames =%lld\n", read_sum); fflush(stdout);
//            break;
//        }
        err = Pa_WriteStream(stream, data, FRAMES_PER_BUFFER / s_info.channels);
        if (err != paNoError) {
            printf("error Pa_WriteStream =%s\n", Pa_GetErrorText(err)); fflush(stdout);
            break;
        }
        memset(data, 0, sizeof(data));
    }

    err = Pa_StopStream(stream);
        if( err != paNoError )
            printf("error Pa_StopStream =%s\n", Pa_GetErrorText(err)); fflush(stdout);

    err = Pa_CloseStream(stream);
    if (err != paNoError)
        printf("error Pa_CloseStream =%s\n", Pa_GetErrorText(err)); fflush(stdout);
    Pa_Terminate();
    sf_close(a_file);
    return 0;
}
