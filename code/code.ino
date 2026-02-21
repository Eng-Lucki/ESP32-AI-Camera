#include <final_one_object_detector_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// --- WIFI & TELEGRAM SETTINGS ---
const char *ssid = "your_wifi_ssid_here";         // Replace with your Wi-Fi SSID
const char *password = "your_wifi_password_here"; // Replace with your Wi-Fi password
String BOTtoken = "your_bot_token_here";          // Replace with your bot token
String CHAT_ID = "your_chat_id_here";             // Replace with your chat ID
bool sendPhoto = false;

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

#define FLASH_LED_PIN 4
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;
bool flashState = LOW;

// GLOBAL VARIABLE TO STORE LAST DETECTION
String last_detection_result = "Camera Initialized - Waiting for detection...";

// --- CAMERA PINS ---
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Button on GPIO 12
#define BUTTON_PIN 12

// --- SETTINGS FOR HIGH QUALITY (REQUIRES PSRAM) ---
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3

// --- OLED SETTINGS ---
#define I2C_SDA 15
#define I2C_SCL 14
TwoWire I2Cbus = TwoWire(0);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cbus, OLED_RESET);

// --- TELEGRAM FUNCTIONS ---

void handleNewMessages(int numNewMessages)
{
    Serial.print("Handle New Messages: ");
    Serial.println(numNewMessages);

    for (int i = 0; i < numNewMessages; i++)
    {
        String chat_id = String(bot.messages[i].chat_id);
        if (chat_id != CHAT_ID)
        {
            bot.sendMessage(chat_id, "Unauthorized user", "");
            continue;
        }

        String text = bot.messages[i].text;
        Serial.println(text);

        String from_name = bot.messages[i].from_name;
        if (text == "/start")
        {
            String welcome = "Welcome , " + from_name + "\n";
            welcome += "Use the following commands:\n";
            welcome += "/photo : takes a new photo\n";
            welcome += "/flash : toggles flash LED \n";
            bot.sendMessage(CHAT_ID, welcome, "");
        }
        if (text == "/flash")
        {
            flashState = !flashState;
            digitalWrite(FLASH_LED_PIN, flashState);
            Serial.println("Change flash LED state");
        }
        if (text == "/photo")
        {
            sendPhoto = true;
            Serial.println("New photo request");
        }
    }
}

String sendPhotoTelegram()
{
    const char *myDomain = "api.telegram.org";
    String getBody = "";

    // 1. Dispose old bufferf
    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);

    // 2. Take a new photo for Telegram
    fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Camera capture failed");
        delay(1000);
        ESP.restart();
        return "Camera capture failed";
    }

    Serial.println("Connect to " + String(myDomain));

    if (clientTCP.connect(myDomain, 443))
    {
        Serial.println("Connection successful");
        Serial.println("Sending Caption: " + last_detection_result);

        String boundary = "RandomNerdTutorials";

        String bodyHead = "--" + boundary + "\r\n" +
                          "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
                          CHAT_ID + "\r\n";

        String bodyCaption = "--" + boundary + "\r\n" +
                             "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
                             last_detection_result + "\r\n";

        String bodyPhoto = "--" + boundary + "\r\n" +
                           "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\n" +
                           "Content-Type: image/jpeg\r\n\r\n";

        String bodyEnd = "\r\n--" + boundary + "--\r\n";

        size_t totalLen = bodyHead.length() + bodyCaption.length() + bodyPhoto.length() + fb->len + bodyEnd.length();

        clientTCP.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");
        clientTCP.println("Host: " + String(myDomain));
        clientTCP.println("Content-Length: " + String(totalLen));
        clientTCP.println("Content-Type: multipart/form-data; boundary=" + boundary);
        clientTCP.println();

        clientTCP.print(bodyHead);
        clientTCP.print(bodyCaption);
        clientTCP.print(bodyPhoto);

        uint8_t *fbBuf = fb->buf;
        size_t fbLen = fb->len;
        for (size_t n = 0; n < fbLen; n = n + 1024)
        {
            if (n + 1024 < fbLen)
            {
                clientTCP.write(fbBuf, 1024);
                fbBuf += 1024;
            }
            else if (fbLen % 1024 > 0)
            {
                size_t remainder = fbLen % 1024;
                clientTCP.write(fbBuf, remainder);
            }
        }

        clientTCP.print(bodyEnd);

        esp_camera_fb_return(fb);

        int waitTime = 10000;
        long startTimer = millis();
        boolean state = false;
        String getAll = "";

        while ((startTimer + waitTime) > millis())
        {
            Serial.print(".");
            delay(100);
            while (clientTCP.available())
            {
                char c = clientTCP.read();
                if (state == true)
                    getBody += String(c);
                if (c == '\n')
                {
                    if (getAll.length() == 0)
                        state = true;
                    getAll = "";
                }
                else if (c != '\r')
                    getAll += String(c);
                startTimer = millis();
            }
            if (getBody.length() > 0)
                break;
        }
        clientTCP.stop();
        Serial.println(getBody);
    }
    else
    {
        getBody = "Connected to api.telegram.org failed.";
        Serial.println("Connected to api.telegram.org failed.");
    }
    return getBody;
}

bool camera_enabled = true;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf;

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA, // QVGA (320x240) - Requires PSRAM
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

void setup()
{
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);

    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, flashState);

    WiFi.mode(WIFI_STA);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print("ESP32-CAM IP Address: ");
    Serial.println(WiFi.localIP());

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    if (psramFound())
    {
        Serial.println("PSRAM is detected and enabled");
    }
    else
    {
        Serial.println("ERR: PSRAM NOT FOUND! Code will likely crash on snapshot.");
    }

    I2Cbus.begin(I2C_SDA, I2C_SCL, 100000);
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println("SSD1306 OLED failed");
    }

    Serial.println("Edge Impulse Inferencing Demo");

    if (ei_camera_init() == false)
    {
        ei_printf("Failed to initialize Camera!\r\n");
        camera_enabled = false;
    }
    else
    {
        ei_printf("Camera initialized\r\n");
        camera_enabled = true;
    }

    ei_printf("\nStarting continuous inference in 2 seconds...\n");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.print("Starting...");
    display.display();
    ei_sleep(2000);
    display.clearDisplay();
}

void loop()
{
    if (sendPhoto)
    {
        Serial.println("Preparing photo");
        sendPhotoTelegram();
        sendPhoto = false;
    }

    if (millis() > lastTimeBotRan + botRequestDelay)
    {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        while (numNewMessages)
        {
            Serial.println("got response");
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastTimeBotRan = millis();
    }

    int reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState)
    {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay)
    {
        static int stableState = HIGH;
        if (reading != stableState)
        {
            stableState = reading;
            if (stableState == LOW)
            {
                camera_enabled = !camera_enabled;
                display.clearDisplay();
                display.setCursor(0, 0);
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                display.print(camera_enabled ? "Camera ON" : "Camera OFF");
                display.display();
            }
        }
    }
    lastButtonState = reading;

    if (!camera_enabled)
    {
        ei_sleep(50);
        return;
    }

    if (ei_sleep(5) != EI_IMPULSE_OK)
    {
        return;
    }

    // ALLOCATE FROM PSRAM
    size_t required_ram = EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE;
    snapshot_buf = (uint8_t *)ps_malloc(required_ram);

    if (snapshot_buf == nullptr)
    {
        ei_printf("ERR: Failed to allocate snapshot buffer! PSRAM free: %d\n", ESP.getFreePsram());
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false)
    {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK)
    {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        free(snapshot_buf);
        return;
    }

    String current_frame_detection = "";

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    bool bb_found = false;
    display.clearDisplay();

    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++)
    {
        ei_impulse_result_bounding_box_t *bb = &result.bounding_boxes[ix];
        if (bb->value == 0)
            continue;

        bb_found = true;

        current_frame_detection += String(bb->label) + " (" + String(int(bb->value * 100)) + "%), ";

        display.setCursor(0, 20 * ix);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.print(bb->label);
        display.print(" ");
        display.print(int((bb->value) * 100));
        display.print("%");
    }

    if (!bb_found)
    {
        display.setCursor(0, 16);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.print("No objects");
        last_detection_result = "No objects detected";
    }
    else
    {
        last_detection_result = "Detected: " + current_frame_detection;
    }

    display.display();

#else
    float max_val = 0;
    String label = "";
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        if (result.classification[ix].value > max_val)
        {
            max_val = result.classification[ix].value;
            label = result.classification[ix].label;
        }
    }
    last_detection_result = "Class: " + label + " (" + String(int(max_val * 100)) + "%)";
#endif

    free(snapshot_buf);
}

bool ei_camera_init(void)
{
    if (is_initialised)
        return true;
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
        return false;
    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID)
    {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }
    is_initialised = true;
    return true;
}

void ei_camera_deinit(void)
{
    if (!is_initialised)
        return;
    esp_camera_deinit();
    is_initialised = false;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf)
{
    bool do_resize = false;
    if (!is_initialised)
        return false;
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
        return false;
    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);
    esp_camera_fb_return(fb);
    if (!converted)
        return false;
    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS) || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS))
        do_resize = true;
    if (do_resize)
        ei::image::processing::crop_and_interpolate_rgb888(out_buf, EI_CAMERA_RAW_FRAME_BUFFER_COLS, EI_CAMERA_RAW_FRAME_BUFFER_ROWS, out_buf, img_width, img_height);
    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;
    while (pixels_left != 0)
    {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}