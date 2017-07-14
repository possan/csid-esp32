/* I2S Example
    
    This example code will output 100Hz sine wave and triangle wave to 2-channel of I2S driver
    Every 5 seconds, it will change bits_per_sample [16, 24, 32] for i2s data 

    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_system.h"

#include <stdio.h>
#include <math.h>

#include <libcsid.h>

#include "xi2c.h"
#include "fonts.h"
#include "ssd1306.h"

#include "commando.inc"

// from ESP32 Audio shield demo

static QueueHandle_t i2s_event_queue;

#define I2C_EXAMPLE_MASTER_SCL_IO    14    /*!< gpio number for I2C master clock */////////////
#define I2C_EXAMPLE_MASTER_SDA_IO    13    /*!< gpio number for I2C master data  *//////////////
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */


#define SAMPLE_RATE     (22050)
#define I2S_CHANNEL        I2S_NUM_0
// #define WAVE_FREQ_HZ    (200)
#define PI 3.14159265

// #define SAMPLE_PER_CYCLE (SAMPLE_RATE / WAVE_FREQ_HZ)
#define HALF_SAMPLERATE (SAMPLE_RATE / 2)

unsigned long phase = 0;

unsigned short waveform_buffer[128] = { 0, };


static void setup_triangle_sine_waves()
{
    int samples = 300;
    unsigned short *mono_samples_data = (unsigned short *)malloc(2 * samples);
    unsigned short *samples_data = (unsigned short *)malloc(2 * 2 * samples);

    unsigned int i, sample_val;
    double sin_float;

    libcsid_render(mono_samples_data, samples);

    // printf("Test phase=%ld, free mem=%d, written data=%d\n", phase, esp_get_free_heap_size(), 2 * 2 * samples);

    for(i = 0; i < samples; i ++) {
        // sin_float = sin((float)phase * PI / HALF_SAMPLERATE) * 10000.0;

        // sample_val = 0;
        // sample_val += (short) sin_float;
        sample_val = mono_samples_data[i];
        samples_data[i * 2 + 0] = sample_val;

        if (i < 128) {
            waveform_buffer[i] = sample_val;
        }


        // sample_val = sample_val << 16;
        // sample_val = mono_samples_data[i];
        samples_data[i * 2 + 1] = sample_val;

        // phase += WAVE_FREQ_HZ;
    }

    int pos = 0;
    int left = 2 * 2 * samples;
    unsigned char *ptr = (unsigned char *)samples_data;

    while (left > 0) {
        int written = i2s_write_bytes(I2S_CHANNEL, (const char *)ptr, left, 100 / portTICK_RATE_MS);
        // printf("%d of %d bytes written.\n", written, left);
        // if (written < left) {
        // printf("buffer overrun, wait a bit...\n");
        // vTaskDelay(5 / portTICK_PERIOD_MS);
        // }

        pos += written;
        ptr += written;
        left -= written;
        // printf("%d bytes written.\n", written);
    }

    free(samples_data);
    free(mono_samples_data);
}










void audiorenderer_loop(void *pvParameter) {
    while(1) {
        // while (1) {
        //     // audioplayer_buffer();
        setup_triangle_sine_waves();
        //     vTaskDelay(5 / portTICK_PERIOD_MS);
        // }
    }
}


// typedef enum {
//     I2S, I2S_MERUS, DAC_BUILT_IN, PDM
// } output_mode_t;


// typedef struct
// {
//     output_mode_t output_mode;
//     int sample_rate;
//     float sample_rate_modifier;
//     i2s_bits_per_sample_t bit_depth;
//     i2s_port_t i2s_num;
// } renderer_config_t;

// typedef enum
// {
//     PCM_INTERLEAVED, PCM_LEFT_RIGHT
// } pcm_buffer_layout_t;

// typedef struct
// {
//     uint32_t sample_rate;
//     i2s_bits_per_sample_t bit_depth;
//     uint8_t num_channels;
//     pcm_buffer_layout_t buffer_format;
// } pcm_format_t;




void renderer_zero_dma_buffer()
{
    i2s_zero_dma_buffer(I2S_CHANNEL);
}

void audioplayer_init()
{
    i2s_mode_t mode = I2S_MODE_MASTER | I2S_MODE_TX;

    i2s_config_t i2s_config = {
        .mode = mode,          // Only TX
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,   // 2-channels
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 32,                            // number of buffers, 128 max.
        .dma_buf_len = 32 * 2,                          // size of each buffer
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1        // Interrupt level 1
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = I2S_PIN_NO_CHANGE    // Not used
    };

    i2s_driver_install(I2S_CHANNEL, &i2s_config, 1, &i2s_event_queue);
    i2s_set_pin(I2S_CHANNEL, &pin_config);
    i2s_set_sample_rates(I2S_CHANNEL, SAMPLE_RATE);
    i2s_stop(I2S_CHANNEL);
}


void audioplayer_start()
{
    // renderer_status = RUNNING;
    i2s_start(I2S_CHANNEL);

    // buffer might contain noise
    i2s_zero_dma_buffer(I2S_CHANNEL);
}




/**
 * @brief i2c master initialization
 */
static void i2c_example_master_init()
{
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                       I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
}


void screen_loop(void *pvParameter) {
    while(1) {
        SSD1306_DrawFilledRectangle(0, 32, 128, 32, SSD1306_COLOR_BLACK);
        short *x = (short *)&waveform_buffer;
        for(int i=0; i<128; i++) {
            SSD1306_DrawPixel(i, 48 + (x[i] / 2000), SSD1306_COLOR_WHITE);
        }
        SSD1306_UpdateScreen();
        vTaskDelay(4 / portTICK_PERIOD_MS);
    }
}


void app_main() {
    // int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;

    // i2c_config_t conf;
    // conf.mode = I2C_MODE_MASTER;
    // conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    // conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    // conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    // conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    // conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;

    // i2c_param_config(i2c_master_port, &conf);

    // i2c_driver_install(i2c_master_port, conf.mode,
    //                    I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
    //                    I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);

    //for 36Khz sample rates, we create 100Hz sine wave, every cycle need 36000/100 = 360 samples (4-bytes or 8-bytes each sample)
    //depend on bits_per_sample
    //using 6 buffers, we need 60-samples per buffer
    //if 2-channels, 16-bit each channel, total buffer is 360*4 = 1440 bytes
    //if 2-channels, 24/32-bit each channel, total buffer is 360*8 = 2880 bytes

    // i2s_config_t i2s_config = {
    //     .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
    //     .sample_rate = SAMPLE_RATE,
    //     .bits_per_sample = 16,
    //     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
    //     .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
    //     .dma_buf_count = 6,
    //     .dma_buf_len = 60,                                                      //
    //     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                                //Interrupt level 1
    // };

    // i2s_pin_config_t pin_config = {
    //     .bck_io_num = 26,
    //     .ws_io_num = 25,
    //     .data_out_num = 22,
    //     .data_in_num = -1                                                       //Not used
    // };

    // i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    // i2s_set_pin(I2S_NUM, &pin_config);
    // i2s_set_clk(I2S_NUM, SAMPLE_RATE, 16, 2);

    printf("-----------------------------------\n");
    printf("HELLO WORLD\n");
    printf("-----------------------------------\n");
    // fflush(stdout);

    i2c_example_master_init();
    SSD1306_Init();


    SSD1306_Fill(SSD1306_COLOR_BLACK); // clear screen

    SSD1306_GotoXY(4, 4);
    SSD1306_Puts("ESP32-SID", &Font_11x18, SSD1306_COLOR_WHITE);

//     SSD1306_GotoXY(2, 20);
// #ifdef CONFIG_BT_SPEAKER_MODE /////bluetooth speaker mode/////
//     SSD1306_Puts("PCM5102 BT speaker", &Font_7x10, SSD1306_COLOR_WHITE);
//     SSD1306_GotoXY(2, 30);
//     SSD1306_Puts("my device name is", &Font_7x10, SSD1306_COLOR_WHITE);
//     SSD1306_GotoXY(2, 39);
//     SSD1306_Puts(dev_name, &Font_7x10, SSD1306_COLOR_WHITE);
//     SSD1306_GotoXY(16, 53);
//     SSD1306_Puts("Yeah! Speaker!", &Font_7x10, SSD1306_COLOR_WHITE);
// #else ////////for webradio mode display////////////////
//     SSD1306_Puts("PCM5102A webradio", &Font_7x10, SSD1306_COLOR_WHITE);
//     SSD1306_GotoXY(2, 30);
    
//     SSD1306_Puts(url, &Font_7x10, SSD1306_COLOR_WHITE);
//     if (strlen(url) > 18)  {
//       SSD1306_GotoXY(2, 39);
//       SSD1306_Puts(url + 18, &Font_7x10, SSD1306_COLOR_WHITE);
//     }
//     SSD1306_GotoXY(16, 53);

//     tcpip_adapter_ip_info_t ip_info;
//     tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
//     SSD1306_GotoXY(2, 53);
//     SSD1306_Puts("IP:", &Font_7x10, SSD1306_COLOR_WHITE);
//     SSD1306_Puts(ip4addr_ntoa(&ip_info.ip), &Font_7x10, SSD1306_COLOR_WHITE);    
// #endif

    /* Update screen, send changes to LCD */
    SSD1306_UpdateScreen();

    /* for class-D amplifier system. Dim OLED to avoid noise from panel*/
    /* PLEASE comment out next three lines for ESP32-ADB system*/  
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    SSD1306_Fill(SSD1306_COLOR_BLACK);
    SSD1306_UpdateScreen();  



    audioplayer_init();
    audioplayer_start();

    libcsid_init(22050, SIDMODEL_6581);
    libcsid_load((unsigned char *)&music_Commando_sid, music_Commando_sid_len, 0);

    printf("SID Title: %s\n", libcsid_gettitle());
    printf("SID Author: %s\n", libcsid_getauthor());
    printf("SID Info: %s\n", libcsid_getinfo());

    SSD1306_GotoXY(2, 2);
    SSD1306_Puts(libcsid_gettitle(), &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 16);
    SSD1306_Puts(libcsid_getauthor(), &Font_7x10, SSD1306_COLOR_WHITE);

    SSD1306_UpdateScreen();

    xTaskCreate(&audiorenderer_loop, "audio", 16384, NULL, 5, NULL);
    xTaskCreate(&screen_loop, "screen", 16384, NULL, 35, NULL);


    // while (1) {
    //     // audioplayer_buffer();
    //     setup_triangle_sine_waves();

    //     vTaskDelay(5 / portTICK_PERIOD_MS);
    // }
}


// void sdl_callback(void* userdata, unsigned char *stream, int bytes) {
//   libcsid_render((unsigned short *)stream, bytes / 2);
// }

// int main (int argc, char *argv[])
// {
//   int samplerate = DEFAULT_SAMPLERATE;
//   int subtune = 0;
//   int sidmodel = 0;
//   int tunelength = -1;
//   unsigned char filedata[MAX_DATA_LEN] = {0, };

//   // open and process the file
//   if (argc < 2) {
//     printf("\nUsage: csid <filename.sid> [subtune_number [SID_modelnumber [seconds]]]\n\n");
//     return 1;
//   }

//   if (argc >= 3) {
//     sscanf(argv[2], "%d", &subtune);
//     subtune --;
//     if (subtune < 0 || subtune > 63) {
//       subtune = 0;
//     }
//   }

//   if (argc >= 4) {
//     sscanf(argv[3], "%d", &sidmodel);
//     if (sidmodel != SIDMODEL_6581 && sidmodel != SIDMODEL_8580) {
//       sidmodel = DEFAULT_SIDMODEL;
//     }
//   } else {
//     sidmodel = DEFAULT_SIDMODEL;
//   }

//   if (argc >= 5) {
//     sscanf(argv[4], "%d", &tunelength);
//   }

//   if (SDL_Init(SDL_INIT_AUDIO) < 0) {
//     fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
//     return 1;
//   }

//   SDL_AudioSpec soundspec = {0, };
//   soundspec.freq = samplerate;
//   soundspec.channels = 1;
//   soundspec.format = AUDIO_S16;
//   soundspec.samples = 32768;
//   soundspec.userdata = NULL;
//   soundspec.callback = sdl_callback;
//   if (SDL_OpenAudio(&soundspec, NULL) < 0) {
//     fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
//     return 2;
//   }

//   libcsid_init(samplerate, sidmodel);

//   FILE *f = fopen(argv[1], "rb");
//   if (f == NULL) {
//     fprintf(stderr, "File not found.\n");
//     return 1;
//   }

//   fseek(f, 0, SEEK_END);
//   int datalen = ftell(f);
//   fseek(f, 0, SEEK_SET);
//   fread(filedata, 1, datalen, f);
//   printf("\n%d bytes read (%s subtune %d)", datalen, argv[1], subtune + 1);
//   fclose(f);

//   libcsid_load(filedata, datalen, subtune);

//   printf("\nTitle: %s    ", libcsid_gettitle());
//   printf("Author: %s    ", libcsid_getauthor());
//   printf("Info: %s", libcsid_getinfo());

//   SDL_PauseAudio(0);

//   fflush(stdin);
//   if (tunelength != -1) {
//     sleep(tunelength);
//   } else {
//     printf("Press Enter to abort playback...\n");
//     getchar();
//   }

//   SDL_PauseAudio(1);
//   SDL_CloseAudio();
//   return 0;
// }


