/* 
 * This file is part of the ezDV project (https://github.com/tmiw/ezDV).
 * Copyright (c) 2022 Mooneer Salem
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstring>

#include "FreeDVTask.h"

#include "esp_dsp.h"
#include "codec2_math.h"
#include "modem_stats.h"

#define FREEDV_ANALOG_NUM_SAMPLES_PER_LOOP 160
#define CURRENT_LOG_TAG ("FreeDV")

namespace ezdv
{

namespace audio
{

FreeDVTask::FreeDVTask()
    : DVTask("FreeDVTask", 15, 47000, 0, 16, pdMS_TO_TICKS(10))
    , AudioInput(2, 2)
    , dv_(nullptr)
    , rText_(nullptr)
    , currentMode_(0)
    , isTransmitting_(false)
    , isEndingTransmit_(false)
    , isActive_(false)
    , samplesBeforeEnd_(0)
    , stats_(nullptr)
{
    registerMessageHandler(this, &FreeDVTask::onSetFreeDVMode_);
    registerMessageHandler(this, &FreeDVTask::onSetPTTState_);
    registerMessageHandler(this, &FreeDVTask::onReportingSettingsUpdate_);
    registerMessageHandler(this, &FreeDVTask::onRequestGetFreeDVMode_);
}

FreeDVTask::~FreeDVTask()
{
    if (dv_ != nullptr)
    {
        if (rText_ != nullptr)
        {
            reliable_text_unlink_from_freedv(rText_);
            reliable_text_destroy(rText_);
            rText_ = nullptr;
        }

        freedv_close(dv_);
        dv_ = nullptr;
    }
}

void FreeDVTask::onTaskStart_()
{
    isActive_ = true;
}

void FreeDVTask::onTaskSleep_()
{
    isActive_ = false;

    if (dv_ != nullptr)
    {
        if (rText_ != nullptr)
        {
            reliable_text_unlink_from_freedv(rText_);
            reliable_text_destroy(rText_);
            rText_ = nullptr;
        }

        freedv_close(dv_);
        dv_ = nullptr;
    }
}

void FreeDVTask::onTaskTick_()
{
    if (!isActive_) return;

    bool syncLed = false;
    int freqOffset = 0;

    //ESP_LOGI(CURRENT_LOG_TAG, "timer tick");

    struct FIFO* codecInputFifo = nullptr;
    struct FIFO* codecOutputFifo = nullptr;
    if (isTransmitting_)
    {
        // Input is microphone, output is radio
        codecInputFifo = getAudioInput(audio::AudioInput::ChannelLabel::USER_CHANNEL);
        codecOutputFifo = getAudioOutput(audio::AudioInput::ChannelLabel::RADIO_CHANNEL);
    }
    else
    {
        // Input is radio, output is microphone
        codecInputFifo = getAudioInput(audio::AudioInput::ChannelLabel::RADIO_CHANNEL);
        codecOutputFifo = getAudioOutput(audio::AudioInput::ChannelLabel::USER_CHANNEL);
    }

    if (dv_ == nullptr)
    {
        // Analog mode, just pipe through the audio.
        short inputBuf[FREEDV_ANALOG_NUM_SAMPLES_PER_LOOP];
        memset(inputBuf, 0, sizeof(inputBuf));

        if (codec2_fifo_free(codecOutputFifo) < FREEDV_ANALOG_NUM_SAMPLES_PER_LOOP) return;
        
        while (!(isTransmitting_ && isEndingTransmit_) && 
               codec2_fifo_used(codecInputFifo) >= FREEDV_ANALOG_NUM_SAMPLES_PER_LOOP)
        {
            codec2_fifo_read(codecInputFifo, inputBuf, FREEDV_ANALOG_NUM_SAMPLES_PER_LOOP);
            codec2_fifo_write(codecOutputFifo, inputBuf, FREEDV_ANALOG_NUM_SAMPLES_PER_LOOP);
        }

        if (isTransmitting_ && isEndingTransmit_)
        {
            // We've finished processing everything that's left, end TX now.
            TransmitCompleteMessage message;
            publish(&message);

            isEndingTransmit_ = false;
            isTransmitting_ = false;
        }
    }
    else
    {
        if (isTransmitting_)
        {
            int numSpeechSamples = freedv_get_n_speech_samples(dv_);
            int numModemSamples = freedv_get_n_nom_modem_samples(dv_);
            short inputBuf[numSpeechSamples];
            short outputBuf[numModemSamples];

            if (codec2_fifo_free(codecOutputFifo) < numModemSamples) return;
        
            while (codec2_fifo_read(codecInputFifo, inputBuf, numSpeechSamples) == 0)
            {
                // Limit the amount of time we spend here so we don't end up
                // stuck transmitting forever.
                if (isEndingTransmit_)
                {
                    samplesBeforeEnd_ -= numSpeechSamples;
                    if (samplesBeforeEnd_ <= 0)
                    {
                        break;
                    }
                }
                //auto timeBegin = esp_timer_get_time();

                freedv_tx(dv_, outputBuf, inputBuf);
                //auto timeEnd = esp_timer_get_time();
                //ESP_LOGI(CURRENT_LOG_TAG, "freedv_tx ran in %d us on %d samples and generated %d samples", (int)(timeEnd - timeBegin), numSpeechSamples, numModemSamples);
                codec2_fifo_write(codecOutputFifo, outputBuf, numModemSamples);
            }
            
            if (isEndingTransmit_ && samplesBeforeEnd_ < numSpeechSamples)
            {
                // We've finished processing everything that's left, end TX now.
                TransmitCompleteMessage message;
                publish(&message);

                isEndingTransmit_ = false;
                isTransmitting_ = false;
            }
        }
        else
        {
            int numSpeechSamples = freedv_get_n_speech_samples(dv_);
            short inputBuf[freedv_get_n_max_modem_samples(dv_)];
            short outputBuf[numSpeechSamples];
            int nin = freedv_nin(dv_);

            if (codec2_fifo_free(codecOutputFifo) < numSpeechSamples) return;
        
            int rv = codec2_fifo_read(codecInputFifo, inputBuf, nin);
            if (rv == 0)
            {
                //auto timeBegin = esp_timer_get_time();

                int nout = freedv_rx(dv_, outputBuf, inputBuf);

                //auto timeEnd = esp_timer_get_time();
                //ESP_LOGI(CURRENT_LOG_TAG, "freedv_rx ran in %lld us on %d samples and generated %d samples", timeEnd - timeBegin, nin, nout);
                codec2_fifo_write(codecOutputFifo, outputBuf, nout);
                nin = freedv_nin(dv_);
            }
        
            syncLed = freedv_get_sync(dv_) > 0;
            
            freedv_get_modem_extended_stats(dv_, stats_);
            freqOffset = stats_->foff;
        }
    }

    // Broadcast current sync state
    FreeDVSyncStateMessage* message = new FreeDVSyncStateMessage(syncLed, freqOffset);
    publish(message);
    delete message;
}

void FreeDVTask::onSetFreeDVMode_(DVTask* origin, SetFreeDVModeMessage* message)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Setting FreeDV mode to %d", (int)message->mode);
    currentMode_ = (int)message->mode;

    if (dv_ != nullptr)
    {
        if (rText_ != nullptr)
        {
            reliable_text_unlink_from_freedv(rText_);
            reliable_text_destroy(rText_);
            rText_ = nullptr;
        }

        modem_stats_close(stats_);
        delete stats_;

        freedv_close(dv_);
        dv_ = nullptr;
    }

    if (message->mode != FreeDVMode::ANALOG)
    {
        int freedvApiMode = 0;
        switch (message->mode)
        {
            case FREEDV_700D:
                freedvApiMode = FREEDV_MODE_700D;
                break;
            case FREEDV_700E:
                freedvApiMode = FREEDV_MODE_700E;
                break;
            case FREEDV_1600:
                freedvApiMode = FREEDV_MODE_1600;
                break;
            default:
                assert(0);
        }

        dv_ = freedv_open(freedvApiMode);
        assert(dv_ != nullptr);
        
        switch (freedvApiMode)
        {
            case FREEDV_MODE_700D:
                freedv_set_eq(dv_, 1);
                freedv_set_clip(dv_, 1);
                freedv_set_tx_bpf(dv_, 1);
                freedv_set_squelch_en(dv_, 1);
                freedv_set_snr_squelch_thresh(dv_, -2.0);  /* squelch at -2.0 dB      */
                break;
            case FREEDV_MODE_700E:
                freedv_set_eq(dv_, 1);
                freedv_set_clip(dv_, 1);
                freedv_set_tx_bpf(dv_, 1);
                freedv_set_squelch_en(dv_, 1);
                freedv_set_snr_squelch_thresh(dv_, 1);  /* squelch at 1.0 dB      */
                break;
            case FREEDV_MODE_1600:
                freedv_set_clip(dv_, 0);
                freedv_set_tx_bpf(dv_, 0);
                freedv_set_squelch_en(dv_, 0);
                break;
            default:
                assert(0);
                freedv_set_clip(dv_, 0);
                freedv_set_tx_bpf(dv_, 0);
                freedv_set_squelch_en(dv_, 1);
                freedv_set_snr_squelch_thresh(dv_, 0.0);  /* squelch at 0.0 dB      */
                break;
        }

        stats_ = new MODEM_STATS();
        assert(stats_ != nullptr);
        modem_stats_open(stats_);

        // Note: reliable_text setup is deferred until we know for sure whether
        // we have a valid callsign saved.
        storage::RequestReportingSettingsMessage requestReportingSettings;
        publish(&requestReportingSettings);
    }
}

void FreeDVTask::onSetPTTState_(DVTask* origin, FreeDVSetPTTStateMessage* message)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Setting FreeDV transmit state to %d", (int)message->pttState);

    if (isTransmitting_ && !message->pttState)
    {
        // Delay ending TX until we've processed what's remaining. This means we'll need
        // to add a bit of silence at the end of the transmission as well depending on 
        // the currently active mode.
        samplesBeforeEnd_ = 2000; // 250ms maximum @ 8000 Hz
        isEndingTransmit_ = true;

        if (dv_ != nullptr)
        {
            auto codecInputFifo = getAudioInput(audio::AudioInput::ChannelLabel::USER_CHANNEL);
            int numSpeechSamples = freedv_get_n_speech_samples(dv_);
            short* tmpBuffer = new short[numSpeechSamples];
            assert(tmpBuffer != nullptr);

            memset(tmpBuffer, 0, sizeof(short) * numSpeechSamples);
            codec2_fifo_write(codecInputFifo, tmpBuffer, numSpeechSamples);
            delete[] tmpBuffer;
        }
    }
    else
    {
        isEndingTransmit_ = false;
        isTransmitting_ = message->pttState;
        if (!isTransmitting_)
        {
            TransmitCompleteMessage message;
            publish(&message);
        }
    }
}

void FreeDVTask::onReportingSettingsUpdate_(DVTask* origin, storage::ReportingSettingsMessage* message)
{
    if (dv_ != nullptr && strlen(message->callsign) > 0)
    {
        ESP_LOGI(CURRENT_LOG_TAG, "Registering reliable_text handler");

        if (rText_ != nullptr)
        {
            reliable_text_unlink_from_freedv(rText_);
            reliable_text_destroy(rText_);
            rText_ = nullptr;
        }

        // Non-null callsign means we should set up reliable_text.
        rText_ = reliable_text_create();
        assert(rText_ != nullptr);

        reliable_text_set_string(rText_, message->callsign, strlen(message->callsign));
        reliable_text_use_with_freedv(rText_, dv_, OnReliableTextRx_, this);
    }
}

void FreeDVTask::OnReliableTextRx_(reliable_text_t rt, const char* txt_ptr, int length, void* state)
{
    // Broadcast receipt to other components that may want it (such as FreeDV Reporter).
    FreeDVTask* thisPtr = (FreeDVTask*)state;
    
    // Get stats so we can provide updated SNR.
    freedv_get_modem_extended_stats(thisPtr->dv_, thisPtr->stats_);
    
    float snr = thisPtr->stats_->snr_est;
    ESP_LOGI(CURRENT_LOG_TAG, "Received TX from %s" /*at %.1f SNR"*/, txt_ptr /*, (float)snr*/);

    FreeDVReceivedCallsignMessage message((char*)txt_ptr, snr);
    thisPtr->publish(&message);

    FreeDVTask* task = (FreeDVTask*)state;
    reliable_text_reset(task->rText_);
}

void FreeDVTask::onRequestGetFreeDVMode_(DVTask* origin, RequestGetFreeDVModeMessage* message)
{
    SetFreeDVModeMessage msg((FreeDVMode)currentMode_);
    origin->post(&msg);
}

}

}

// Implement required Codec2 math methods below as CMSIS doesn't work on ESP32.
extern "C"
{
    void codec2_dot_product_f32(float* left, float* right, size_t len, float* result)
    {
        dsps_dotprod_f32(left, right, result, len);
    }

    void codec2_complex_dot_product_f32(COMP* left, COMP* right, size_t len, float* resultReal, float* resultImag)
    {
        float realTimesRealResult = 0; // ac
        float realTimesImag1Result = 0; // bc
        float realTimesImag2Result = 0; // ad
        float imagTimesImagResult = 0; // bi * di
        
        dsps_dotprode_f32((float*)left, (float*)right, &realTimesRealResult, len, 2, 2);
        dsps_dotprode_f32((float*)left + 1, (float*)right, &realTimesImag1Result, len, 2, 2);
        dsps_dotprode_f32((float*)left, (float*)right + 1, &realTimesImag2Result, len, 2, 2);
        dsps_dotprode_f32((float*)left + 1, (float*)right + 1, &imagTimesImagResult, len, 2, 2);
        
        *resultReal = realTimesRealResult - imagTimesImagResult;
        *resultImag = realTimesImag1Result + realTimesImag2Result;
    }

    /* Required memory allocation wrapper for embedded platforms. For ezDV, we want to allocate as much as possible
    on external RAM. */

    void* codec2_malloc(size_t size)
    {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
    }

    void* codec2_calloc(size_t nmemb, size_t size)
    {
        return heap_caps_calloc(nmemb, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
    }

    void codec2_free(void* ptr)
    {
        heap_caps_free(ptr);
    }
}
