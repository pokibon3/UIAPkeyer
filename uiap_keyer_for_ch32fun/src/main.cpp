//==================================================================//
// UIAP_keyer_for_ch32fun
// BASE Software from : https://www.gejigeji.com/?page_id=1045 
// Modified by Kimio Ohe JA9OIR/JA1AOQ
//	- port to ch32fun library
//==================================================================
#include "ch32fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include <stdio.h>
#include <stdint.h>

//スクイズ状態
#define SQZ_FREE 0
#define SQZ_SPC0 1
#define SQZ_SPC 2
#define SQZ_DOT0 3
#define SQZ_DOT 4
#define SQZ_DAH_CONT0 5
#define SQZ_DAH_CONT1 6
#define SQZ_DAH_CONT 7
#define SQZ_DASH 8

//パドル状態
#define PDL_DOT 1
#define PDL_DASH 2
#define PDL_FREE 0

#define SQUEEZE_TYPE 0  //スクイーズモード
#define PDL_RATIO 4 //短点・長点比率

#define WPM_MAX 40  //スピード最大値(WPM)
#define WPM_MIN 5   //スピード最小値(WPM)

// ==== 変数 ====
int key_spd = 1000;
int wpm = 20;
bool tone_enabled = false;
int squeeze = 0;
int paddle = PDL_FREE;
int paddle_old = PDL_FREE;

// ==== パドル処理 ====
uint8_t job_paddle() {
	static uint32_t left_time = 0;
	uint8_t key_dot, key_dash;

	key_dot = (!GPIO_digitalRead(PIN_DOT) || !GPIO_digitalRead(PIN_SWA));
	key_dash = (!GPIO_digitalRead(PIN_DASH) || !GPIO_digitalRead(PIN_SWB));

	if (left_time != 0) {
    	left_time--;
  	} else {
    	left_time = key_spd / 2;
    	if (squeeze != SQZ_FREE) {
      	squeeze--;
    	}
  	}

  	if (squeeze != SQZ_FREE) {
    	if (paddle_old == PDL_DOT && key_dash) paddle = PDL_DASH;
    	else if (paddle_old == PDL_DASH && key_dot) paddle = PDL_DOT;
  	}

  	if (SQUEEZE_TYPE == 0) {
    	if (squeeze > SQZ_DASH) paddle = PDL_FREE;
  	} else {
    	if (squeeze > SQZ_SPC) paddle = PDL_FREE;
  	}

  	if (squeeze > SQZ_SPC) return 1;
  	else if (squeeze == SQZ_SPC || squeeze == SQZ_SPC0) return 0;

  	if (paddle == PDL_FREE) {
    	if (key_dot) paddle = PDL_DOT;
    	else if (key_dash) paddle = PDL_DASH;
  	}

  	if (paddle == PDL_FREE) return 0;
  	else {
    	if (paddle == PDL_DOT) squeeze = SQZ_DOT;
    	else {
      	uint8_t dash_len = (SQZ_SPC * PDL_RATIO + 5) / 2;
      	squeeze = SQZ_SPC + dash_len;
    	}
    	left_time = key_spd / 2;
    	paddle_old = paddle;
    	paddle = PDL_FREE;
    	return 1;
  	}

  	return 0;
}

// ==== トーン制御ON ====
void startTone() {
	if (tone_enabled) return;
	tone_enabled = true;
	start_pwm();
	GPIO_digitalWrite(PIN_KEYOUT, high);  // ON
}

// ==== トーン制御OFF ====
void stopTone() {
	tone_enabled = false;
	stop_pwm();
	GPIO_digitalWrite(PIN_KEYOUT, low);   // OFF
}

static inline long map(long x,
                              long in_min, long in_max,
                              long out_min, long out_max)
{
  // Arduino本家と同じくゼロ除算チェックはしない（in_max == in_min だと未定義）
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
// ==== ADCからスピード読み込み ====
void update_speed_from_adc() {
	int adc = GPIO_analogRead(GPIO_Ain0_A2);        // 0 .. 1023
	wpm = map(adc, 0, 1023, WPM_MIN, WPM_MAX);
	key_spd = 4687 / wpm;  // = (1200/wpm) /0.256  4687.5 -> 4687 
	//printf("WPM=%d, SPD=%d\r\n", wpm, key_spd);
	Delay_Ms(10);
}

// ==== キーダウン ====
void keydown() {
  	startTone();
}

// ==== キーアップ ====
void keyup() {
  	stopTone();
}

void TIM1_UP_IRQHandler(void)
{
	// フラグのクリア
	TIM1->INTFR &= (uint16_t)~TIM_IT_Update;
	if (job_paddle()) {
		keydown();
  	}  else {
		keyup();
  	}
}

int main(void)
{
    SystemInit();
	GPIO_setup();				// gpio Setup;    
	GPIO_ADCinit();
	tim1_int_init();			//
	tim2_pwm_init();             // TIM2 PWM Setup
    while (1)
    {
  		update_speed_from_adc(); // スピード調整用
        Delay_Ms(100);
    }
}
