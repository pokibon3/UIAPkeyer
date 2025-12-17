//==================================================================//
// UIAP_keyer_for_ch32fun
// BASE Software from : https://www.gejigeji.com/?page_id=1045 
// Modified by Kimio Ohe JA9OIR/JA1AOQ
//	- port to ch32fun library
//==================================================================
#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include <stdio.h>
#include <stdint.h>
#define SSD1306_128X64
#include "ssd1306_i2c.h"
#include "ssd1306.h"

//スクイズ状態（半ディット単位の状態機械）
#define SQZ_FREE 0
#define SQZ_SPC0 1
#define SQZ_SPC  2
#define SQZ_DOT0 3
#define SQZ_DOT  4
#define SQZ_DAH_CONT0 5
#define SQZ_DAH_CONT1 6
#define SQZ_DAH_CONT  7
#define SQZ_DASH 8

//パドル状態
#define PDL_DOT  1
#define PDL_DASH 2
#define PDL_FREE 0

#define SQUEEZE_TYPE 0  //スクイーズモード
#define PDL_RATIO 4     //短点・長点比率

#define WPM_MAX 40
#define WPM_MIN 5

// ===================== メッセージ =====================
const char *msgs[] = {
  " CQ CQ CQ DE JA1AOQ PSE K   ",
  "JA1AOQ DE JA9OIR K   ",
  "JA9OIR DE JA1AOQ GM TNX FER UR CALL b UR RST 599 ES QTH SAGAMIHARA CITY ES NAME KIMIWO HW? JA9OIR DE JA1AOQ k   ",
  "R JA1AOQ DE JA9OIR GM DR KIMIWO SAN TKS FB REPT b UR RST 599 ES QTH UOZU CITY ES NAME POKI HW? JA1AOQ DE JA9OIR k   ",
  "R DE JA1AOQ TNX FB 1ST QSO, HPE CU AGN DR POKI SAN 73 JA9OIR DE JA1AOQ TU v E E   ",
  "DE JA9OIR TNX FB QSO ES HVE A NICE DAY KIMI SAN 73 a JA1AOQ DE JA9OIR TU v E E   ",
};
#define MSG_COUNT (sizeof(msgs) / sizeof(msgs[0]))

// ==== 変数 ====
int  key_spd = 1000;
int  wpm = 20;
bool tone_enabled = false;
int squeeze = 0;
int paddle = PDL_FREE;
int paddle_old = PDL_FREE;

// ==== 自動送信制御 ====
// 起動時は何もしない
volatile bool auto_mode  = false; // 今、自動送信中か
volatile bool auto_armed = false; // SWAを一度でも押したか（trueで “自動送信機能が有効”）
volatile bool req_start_auto = false;
volatile bool req_reset_auto = false;

// 停止理由（ラッチ停止）
enum StopReason : uint8_t {
  STOP_NONE   = 0,
  STOP_PADDLE = 1, // DOT/DASHで停止（ラッチ）
  STOP_SWB    = 2, // SWBで停止（ラッチ）
};
volatile StopReason stop_reason = STOP_NONE;

// ===================== Morse テーブル =====================
static const char* morseForChar(char c) {
  // 小文字 b は “BT”(-...-) 扱い（区切り用途）
  if (c == 'b') return "-...-";
	if (c == 'a') return ".-.-.";
  if (c == 'k') return "-.--.";
  if (c == 'v') return "...-.-";
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');

  switch (c) {
    // Letters
    case 'A': return ".-";
    case 'B': return "-...";
    case 'C': return "-.-.";
    case 'D': return "-..";
    case 'E': return ".";
    case 'F': return "..-.";
    case 'G': return "--.";
    case 'H': return "....";
    case 'I': return "..";
    case 'J': return ".---";
    case 'K': return "-.-";
    case 'L': return ".-..";
    case 'M': return "--";
    case 'N': return "-.";
    case 'O': return "---";
    case 'P': return ".--.";
    case 'Q': return "--.-";
    case 'R': return ".-.";
    case 'S': return "...";
    case 'T': return "-";
    case 'U': return "..-";
    case 'V': return "...-";
    case 'W': return ".--";
    case 'X': return "-..-";
    case 'Y': return "-.--";
    case 'Z': return "--..";

    // Digits
    case '0': return "-----";
    case '1': return ".----";
    case '2': return "..---";
    case '3': return "...--";
    case '4': return "....-";
    case '5': return ".....";
    case '6': return "-....";
    case '7': return "--...";
    case '8': return "---..";
    case '9': return "----.";

    // Punctuation (必要そうなのだけ)
    case '.': return ".-.-.-";
    case ',': return "--..--";
    case '?': return "..--..";
    case '/': return "-..-.";
    case '=': return "-...-";
    case '+': return ".-.-.";
    case '-': return "-....-";
    case '@': return ".--.-.";

    default: return nullptr;
  }
}

#define FONT_WIDTH 12
#define FONT_COLOR 1
#define LINE_HEIGHT 16
#define FONT_SCALE_16X16 fontsize_16x16
const int colums = 10; /// have to be 16 or 20

int lcdindex = 0;
uint8_t line1[colums];
uint8_t line2[colums];
uint8_t lastChar = 0;

//==================================================================
//	printasc : print the ascii code to the lcd
//==================================================================
static void printAsc(int8_t asciinumber)
{
	if (lcdindex > colums - 1){
		lcdindex = 0;
		for (int i = 0; i <= colums - 1 ; i++){
			ssd1306_drawchar_sz(i * FONT_WIDTH , LINE_HEIGHT, line2[i], FONT_COLOR, FONT_SCALE_16X16);
			line2[i] = line1[i];
		}
		for (int i = 0; i <= colums - 1 ; i++){
			ssd1306_drawchar_sz(i * FONT_WIDTH , LINE_HEIGHT * 2, line1[i], FONT_COLOR, FONT_SCALE_16X16);
			ssd1306_drawchar_sz(i * FONT_WIDTH , LINE_HEIGHT * 3, 32, FONT_COLOR, FONT_SCALE_16X16);
		}
 	}
	line1[lcdindex] = asciinumber;
	ssd1306_drawchar_sz(lcdindex * FONT_WIDTH , LINE_HEIGHT * 3, asciinumber, FONT_COLOR, FONT_SCALE_16X16);
    ssd1306_refresh();
	lcdindex += 1;
}

//==================================================================
//	printascii : print the ascii code to the lcd
//==================================================================
static void printAscii(int8_t c)
{
    switch (c) {
        case 'b': // BT
            printAsc('B');
            printAsc('T');
            break;
        case 'a':   // AR
            printAsc('A');
            printAsc('R');
            break;
        case 'k':   // KN
            printAsc('K');
            printAsc('N');
            break;
        case 'v':   // VA
            printAsc('V');
            printAsc('A');
            break;
        default:
            printAsc(c);
            break;
    }
}

// ===================== 手動パドル処理（SWA/SWBは混ぜない） =====================
uint8_t job_paddle() {
  static uint32_t left_time = 0;
  uint8_t key_dot, key_dash;

  key_dot  = (!GPIO_digitalRead(PIN_DOT));
  key_dash = (!GPIO_digitalRead(PIN_DASH));

  if (left_time != 0) {
    left_time--;
  } else {
    left_time = key_spd / 2;
    if (squeeze != SQZ_FREE) squeeze--;
  }

  if (squeeze != SQZ_FREE) {
    if (paddle_old == PDL_DOT  && key_dash) paddle = PDL_DASH;
    else if (paddle_old == PDL_DASH && key_dot) paddle = PDL_DOT;
  }

  if (SQUEEZE_TYPE == 0) {
    if (squeeze > SQZ_DASH) paddle = PDL_FREE;
  } else {
    if (squeeze > SQZ_SPC)  paddle = PDL_FREE;
  }

  if (squeeze > SQZ_SPC) return 1;
  else if (squeeze == SQZ_SPC || squeeze == SQZ_SPC0) return 0;

  if (paddle == PDL_FREE) {
    if (key_dot) paddle = PDL_DOT;
    else if (key_dash) paddle = PDL_DASH;
  }

  if (paddle == PDL_FREE) return 0;

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

// ===================== 自動送信処理（半ディット単位） =====================
uint8_t job_auto() 
{
    static uint32_t left_time = 0;
    static int auto_squeeze = 0;
    static int gap_half = 0;
    static uint8_t msg_i = 0;
    static uint16_t pos = 0;

    static const char* seq = nullptr;
    static uint8_t elem = 0;

    static bool pending_jump = false;
    static uint8_t pending_msg = 0;
    static uint16_t pending_pos = 0;
    static int pending_gap_half = 0;

    if (req_reset_auto) {
        req_reset_auto = false;
        left_time = 0;
        auto_squeeze = 0;
        gap_half = 0;
        msg_i = 0;    // msgs[0]へ
        pos = 0;
        seq = nullptr;
        elem = 0;
        pending_jump = false;
        pending_gap_half = 0;
    }

    if (left_time != 0) {
        left_time--;
    } else {
        left_time = key_spd / 2;
        if (auto_squeeze != 0) auto_squeeze--;
        else if (gap_half > 0) gap_half--;
    }

    uint8_t out = (auto_squeeze > SQZ_SPC) ? 1 : 0;
    if (auto_squeeze != 0 || gap_half > 0) return out;

    if (pending_jump) {
        pending_jump = false;
        msg_i = pending_msg;
        pos   = pending_pos;
        gap_half = pending_gap_half;
        pending_gap_half = 0;
        seq = nullptr;
        elem = 0;
        return 0;
    }

    const char* msg = msgs[msg_i];
    char c = msg[pos];
    if (c == '\0') {
        msg_i = (uint8_t)((msg_i + 1) % MSG_COUNT);
        pos = 0;
        gap_half = 12; // 単語間追加6dit
        return 0;
    }

    if (c == ' ') {
        while (msg[pos] == ' ') pos++;
        gap_half = 12;
        return 0;
    }

    if (seq == nullptr) {
        printAscii(c);  
        seq = morseForChar(c);
        elem = 0;
        if (seq == nullptr) {
        pos++;
        gap_half = 12;
        return 0;
        }
    }

    char e = seq[elem];
    if (e == '\0') {
        seq = nullptr;
        elem = 0;
        pos++;
        gap_half = 4; // 文字間追加2dit
        return 0;
    }

    // 要素をスケジュール（要素+要素間1dit OFFを含む）
    if (e == '.') auto_squeeze = SQZ_DOT;   // 1dit ON + 1dit OFF
    else          auto_squeeze = SQZ_DASH;  // 3dit ON + 1dit OFF
    elem++;

    // 最後の要素なら、次の文字に応じた追加ギャップを予約
    if (seq[elem] == '\0') {
        uint16_t look = pos + 1;
/*
        if (msg[look] == ' ') {
            printAscii(32);
            while (msg[look] == ' ') look++;
            if (msg[look] == '\0') {
                pending_msg = (uint8_t)((msg_i + 1) % MSG_COUNT);
                pending_pos = 0;
            } else {
                pending_msg = msg_i;
                pending_pos = look;
            }
            pending_gap_half = 12;
            pending_jump = true;
*/
        if (msg[look] == ' ') {
            // 連続スペース数を数える
            uint8_t nsp = 0;
            while (msg[look] == ' ') { nsp++; look++; }

            // LCD表示もスペース数分進めたいならループ（重いので注意）
            // for (uint8_t i = 0; i < nsp; i++) printAscii(32);
            printAscii(32); // とりあえず1回だけ表示（表示は好みで）

            // 次の送信位置（スペースの次へ）
            if (msg[look] == '\0') {
                pending_msg = (uint8_t)((msg_i + 1) % MSG_COUNT);
                pending_pos = 0;
            } else {
                pending_msg = msg_i;
                pending_pos = look;
            }

            // ★スペース数ぶん待つ：1スペース=7dit
            // auto_squeezeに含まれる「最後の要素後の1dit OFF」を差し引いて -2(half-dit)
            pending_gap_half = (int)(14 * nsp - 2);   // nsp=1→12, nsp=2→26, nsp=3→40 ...
            pending_jump = true;
        } else if (msg[look] == '\0') {
        pending_msg = (uint8_t)((msg_i + 1) % MSG_COUNT);
        pending_pos = 0;
        pending_gap_half = 12;
        pending_jump = true;
        } else {
        pending_msg = msg_i;
        pending_pos = look;
        pending_gap_half = 4;
        pending_jump = true;
        }
    }

    return out;
}

// ===================== トーン制御 =====================
void startTone() 
{
    if (tone_enabled) return;
    tone_enabled = true;
	start_pwm();
	GPIO_digitalWrite(PIN_KEYOUT, high);  // ON
}

void stopTone() 
{
	stop_pwm();
    tone_enabled = false;
	GPIO_digitalWrite(PIN_KEYOUT, low);   // OFF
}

inline void keydown() { startTone(); }
inline void keyup()   { stopTone();  }

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


// ===================== タイマー割り込み =====================
void TIM1_UP_IRQHandler(void)
{
  
    bool dotPressed  = !GPIO_digitalRead(PIN_DOT);
    bool dashPressed = !GPIO_digitalRead(PIN_DASH);
    bool swaPressed  = !GPIO_digitalRead(PIN_SWA);
    bool swbPressed  = !GPIO_digitalRead(PIN_SWB);

    // フラグのクリア
	  TIM1->INTFR &= (uint16_t)~TIM_IT_Update;

    bool allReleased = (!dotPressed && !dashPressed && !swaPressed && !swbPressed);
    static bool prevAllReleased = true; // power-on直後に開始しないため true

    // --- SWB 押下エッジ検出（ラッチ停止） ---
    static bool prevSWB = false;
    if (!prevSWB && swbPressed) {
        auto_mode = false;
        stop_reason = STOP_SWB;   // ラッチ
        keyup();                  // 即ミュート（自動送信途中でも切る）
    }
    prevSWB = swbPressed;

    // --- SWA：ラッチ解除して先頭から開始 ---
    if (req_start_auto) {
        req_start_auto = false;
        auto_armed = true;
        auto_mode = true;
        stop_reason = STOP_NONE;  // 解除
        req_reset_auto = true;    // msgs[0]へ
    }

    // --- DOT/DASH：自動送信を停止（ラッチ）---
    if (auto_mode && (dotPressed || dashPressed)) {
        auto_mode = false;
        stop_reason = STOP_PADDLE; // ★ラッチ（離しても再開しない）
        keyup();                   // 自動送信途中でも即ミュート
    }

    // --- 何も押されない → 自動再開 ---
    // ★STOP_SWB / STOP_PADDLE のどちらでも再開しない（SWAのみで再開）
    if (auto_armed && allReleased && !prevAllReleased) {
        if (stop_reason == STOP_NONE) {
            auto_mode = true;
            req_reset_auto = true;  // msgs[0]先頭へ
        }
    }
    prevAllReleased = allReleased;

    // 出力（ラッチ停止中でも手動パドルは有効）
    uint8_t on = auto_mode ? job_auto() : job_paddle();
    if (on) {
      keydown();
    } else {
        keyup();
    }
}
//
//  main loop
//
int main()
{
    SystemInit();
  	ssd1306_i2c_init();
	ssd1306_init();
  	GPIO_setup();				// gpio Setup;    
	GPIO_ADCinit();
	tim1_int_init();			//
	tim2_pwm_init();             // TIM2 PWM Setup
//    for (uint8_t c = 0x00; c <= 0xff; c++){
//        ssd1306_drawchar_sz(36,0, c, 1, fontsize_64x64);
//        ssd1306_refresh();
//        Delay_Ms(100);
//    }
	Delay_Ms(1000);
    ssd1306_drawstr_sz(4, 0, "UIAP KEYER V1.0", 1, fontsize_8x8);
    ssd1306_drawFastHLine(0, 10, 128, 1);
    ssd1306_refresh();
    while (1)
    {
        // SWAだけデバウンスして「押した瞬間」を拾う
        static uint32_t tA = 0;
        static int lastA = high;
        const uint32_t DEB = 30;

  		update_speed_from_adc(); // スピード調整用
        int a = GPIO_digitalRead(PIN_SWA);
        uint32_t now = millis();

        if (a != lastA && (now - tA) > DEB) {
            tA = now;
            lastA = a;
            if (a == low) req_start_auto = true;
        }

        Delay_Ms(5);
    }
}
