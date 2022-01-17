/*
 * Flicker Meter
 * 
 * Board: Seeeduino Wio Terminal
 *
 * Library:
 *    https://github.com/lovyan03/LovyanGFX
 *    https://www.arduino.cc/reference/en/libraries/arduinofft/
 */

#define FFT_FREQ    20480
#define FFT_SAMPLE  512

#include "SAMD51_TC.h"
#include "arduinoFFT.h"

#define LGFX_WIO_TERMINAL
#define LGFX_USE_V1
#include "LovyanGFX.h"
#include "LGFX_AUTODETECT.hpp"

arduinoFFT FFT = arduinoFFT();

static LGFX tft;

volatile int timeout = 0;
double vReal[FFT_SAMPLE];
double vImag[FFT_SAMPLE];
int count = 0, fft_div = 2;
volatile int light_flg = 0;
int buf_dat[FFT_SAMPLE / 2];
int mode = 0;


void isrTimer () {
  static int w = 0;

  w ++;
  if (w >= fft_div) {
    w = 0;

    if (! light_flg) {
      if (mode == 0) {
        vReal[count] = (double)analogRead(WIO_LIGHT) / 1024.0;
      } else {
        vReal[count] = (double)analogRead(WIO_MIC) / 1024.0;
      }
      vImag[count] = 0;
      count ++;
      if (count >= FFT_SAMPLE) {
        count = 0;
        light_flg = 1;
      }
    }
  }

  pollSwPin();
  if (timeout) timeout --;
}

void setup() {

  Serial.begin(115200);
  ioInitPin();
  pinMode(PIN_LED, OUTPUT);
  pinMode(CMD_KEY_A, INPUT_PULLUP);
  pinMode(CMD_KEY_B, INPUT_PULLUP);
  pinMode(CMD_KEY_C, INPUT_PULLUP);
  pinMode(CMD_5S_PRESS, INPUT_PULLUP);
  digitalWrite(PIN_LED, HIGH);
  pinMode(WIO_LIGHT, INPUT); // PD15-22C/TR8
  pinMode(WIO_MIC, INPUT); // ECM

  tft.begin();
  tft.setRotation(2); // 240 x 320
  setFreq(fft_div);

  TimerTC1.initialize(1000000 / FFT_FREQ);
  TimerTC1.attachInterrupt(isrTimer);
}

void setFreq (int n) {
  int i;

  __disable_irq();
  count = 0;
  fft_div = n;
  light_flg = 0;
  __enable_irq();

  tft.startWrite();
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setFont(&fonts::Font0);
  for (i = 0; i < FFT_SAMPLE / 2; i += 25) {
    tft.drawLine(45, 32 + i, 50, 32 + i, TFT_WHITE);
    tft.setCursor(5, 32 + i - 4);
    tft.printf("%5d", (FFT_FREQ / fft_div / 2 * i) / (FFT_SAMPLE / 2));
  }
  tft.drawRect(50, 30, 154, 260, TFT_DARKGREY);
  tft.setCursor(20, 290);
  tft.printf("(Hz)");

  tft.setFont(&fonts::Font2);
  tft.setCursor(70, 5);
  if (mode == 0) {
    tft.printf("FLICKER METER");
  } else {
    tft.printf("AUDIO ANALYZER");
  }
  tft.setFont(&fonts::Font2);
  tft.setCursor(180, 300);
  tft.printf("%2dk sps", FFT_FREQ / fft_div / 1000);
  tft.endWrite();
}

void loop() {
  int i;
  double peak;

  if (digitalRead(CMD_KEY_A) == LOW) {
    setFreq(1);
    while (digitalRead(CMD_KEY_A) == LOW);
  }
  if (digitalRead(CMD_KEY_B) == LOW) {
    setFreq(2);
    while (digitalRead(CMD_KEY_B) == LOW);
  }
  if (digitalRead(CMD_KEY_C) == LOW) {
    setFreq(10);
    while (digitalRead(CMD_KEY_C) == LOW);
  }
  if (digitalRead(CMD_5S_PRESS) == LOW) {
    mode = ! mode;
    setFreq(1);
    while (digitalRead(CMD_5S_PRESS) == LOW);
  }

  if (light_flg) {
    FFT.Windowing(vReal, FFT_SAMPLE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, FFT_SAMPLE, FFT_FORWARD); // calculate FT
    FFT.ComplexToMagnitude(vReal, vImag, FFT_SAMPLE); // mag. to vR 1st half
    peak = FFT.MajorPeak(vReal, FFT_SAMPLE, FFT_FREQ / fft_div); // get peak freq

    tft.startWrite();
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(20, 300);
    tft.printf("Peak: %4d Hz ", (int)peak);

    for (i = 1; i < FFT_SAMPLE / 2; i ++) {
      tft.drawLine(52, 32 + i, 52 + buf_dat[i], 32 + i, TFT_NAVY);

      buf_dat[i] = vReal[i];
      if (buf_dat[i] > 150) buf_dat[i] = 150;
      tft.drawLine(52, 32 + i, 52 + buf_dat[i], 32 + i, TFT_GREEN);
    }
    light_flg = 0;
    tft.endWrite();
  }
}
