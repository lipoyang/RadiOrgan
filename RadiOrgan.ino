// このスケッチはGR-CITRUS専用です。
#include "PWMeter4Citrus.h"
#include "Servo.h"

// パルス幅[usec]
#define PW_MIN      1100 // 最小
#define PW_NEU      1500 // 中立
#define PW_MAX      1900 // 最大
#define PW_AMP       400 // 振幅
#define PW_AUX       250 // AUXチャンネルの振幅の2値化閾値

// THチャンネルだけパルス幅が異なる (7:3モード、実測値)
#define TH_MIN      1050 // 最小
#define TH_NEU      1363 // 中立
#define TH_MAX      1850 // 最大
#define TH_AMP       480 // 振幅
#define TH_PLAY       10 // あそび

// ピン番号定数
#if 1
#define PIN_OCTAVE      2  // オクターブ表示出力
#define PIN_KEY         3  // キー表示出力
#define PIN_VOL         4  // 音量表示出力
#else
#define PIN_OCTAVE      0  // オクターブ表示出力
#define PIN_KEY         1  // キー表示出力
#define PIN_VOL         5  // 音の強さ表示出力
#endif
#define PIN_LED         9  // 受信表示LED出力
#define PIN_SH          10 // STチャンネル入力
#define PIN_TH          11 // THチャンネル入力
#define PIN_AUX1        12 // AUX1チャンネル入力
#define PIN_AUX2        13 // AUX2チャンネル入力

// パルス幅計測器
PWMeter pwmeterST;      // STチャンネル
PWMeter pwmeterTH;      // THチャンネル
PWMeter pwmeterAUX1;    // AUX1チャンネル
PWMeter pwmeterAUX2;    // AUX2チャンネル

// キーの定数
#define KEY_C           0   // C
#define KEY_C_SHARP     1   // C#
#define KEY_D           2   // D
#define KEY_D_SHARP     3   // D#
#define KEY_E           4   // E
#define KEY_F           5   // F
#define KEY_F_SHARP     6   // F#
#define KEY_G           7   // G
#define KEY_G_SHARP     8   // G#
#define KEY_A           9   // A
#define KEY_A_SHARP     10  // A#
#define KEY_B           11  // B

// 音階 (ハ長調) テーブル
int SCALE[7] = {KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_A, KEY_B};

// 各キーの音波の周期のタイマカウント値テーブル
// (scale.xlsxで計算)
uint16_t KEY_COUNT[12] = {
    11467,      // C
    10823,      // C#
    10216,      // D
    9642,       // D#
    9101,       // E
    8590,       // F
    8108,       // F#
    7653,       // G
    7224,       // G#
    6818,       // A
    6436,       // A#
    6074        // B
};

// STチャンネルのキー判定閾値テーブル
// ギターのフレットのようなもの。これにより離散的な音階になる。
// (実行時にsetup()で計算)
uint16_t FRET[6];

// メータ表示用サーボの角度定数 (実機に合わせて微調整)
#define POS_OCT3    (120-2)  // オクターブ3
#define POS_OCT4    (90-2)  // オクターブ4
#define POS_OCT5    (60-2) // オクターブ5
#define POS_KEY_C   150 // キー:C
#define POS_KEY_B   35  // キー:B
#define POS_VOL0    130  // 音量ゼロ
#define POS_VOL100  25 // 音量最大

// 各オクターブの表示角度
int POS_OCTAVE[3] = {POS_OCT3, POS_OCT4, POS_OCT5};
// 各キーの表示角度 (実行時にsetup()で計算)
int POS_KEY[7];

// メータ表示用サーボ
Servo servoOctave;  // オクターブ
Servo servoKey;     // キー
Servo servoVol;     // 音量

// 受信中か？
bool receiving = false;

// 音量テーブル (指数関数)
int VOL_TABLE[9] = {
    0,
    60,
    90,
    135,
    202,
    303,
    455,
    683,
    1024
};

// ビープ音の初期化
void beep_init()
{
    //MTU0を使用し、#7ピン (P32/MTIOC0C) に出力
    
    // P32をMTIOC0Cに設定
    MPC.PWPR.BIT.B0WI = 0;      // ピン機能選択の
    MPC.PWPR.BIT.PFSWE = 1;     // 　プロテクト解除
    MPC.P32PFS.BIT.PSEL = 0x01; // P32をMTIOC0Cに設定
    MPC.PWPR.BIT.PFSWE = 0;     // ピン機能選択の
    MPC.PWPR.BIT.B0WI = 1;      // 　プロテクト設定
    PORT3.PMR.BIT.B2 = 1;       // P32は周辺機能として使用

    SYSTEM.PRCR.WORD = 0xA502;  // プロテクト解除
    MSTP(MTU0) = 0;             // MTU0のストップ状態解除
    SYSTEM.PRCR.WORD = 0xA500;  // プロテクト設定
    
    // MTU0のTCNT動作停止
    MTU.TSTR.BIT.CST0 = 0;

    // TCNTクリア
    MTU0.TCNT = 0x0000;

    // カウンタクロックとカウンタクリア要因の設定
    // GR-CITRUSでは、外部発振子 12MHz, 
    // PLLCR.STC=16(16逓倍), PLLCR.PLIDIV=0(1分周), SCKCR.PCKB=2(4分周)
    // より、MTUのPCLKは 12*16/4 = 48MHz
    MTU0.TCR.BIT.TPSC = 2;  // カウンタクロック: PCLK/16 = 3MHz
    MTU0.TCR.BIT.CKEG = 0;  // 立ち上がりエッジでカウント
    MTU0.TCR.BIT.CCLR = 5;  // TGRCのコンペアマッチでTCNTクリア

    // PWMモード1
    MTU0.TMDR.BYTE = 0x02;
    
    // 波形出力レベルの選択
    MTU0.TIORL.BIT.IOC = 1; // 初期状態で0出力 周期コンペアマッチで0出力
    MTU0.TIORL.BIT.IOD = 2; // デューティコンペアマッチで1出力

    // 周期とデューティーの設定
    MTU0.TGRC = 6818;//KEY_COUNT[KEY_F];   // 周期
    MTU0.TGRD = 3409;//0;                  // デューティー
    
    // MTU0のTCNT動作開始
    MTU.TSTR.BIT.CST0 = 1;
}

// ビープ音の出力
// key: キー
// octave: オクターブ (-1/0/+1)
// vol: 音量 (0～1024)
void beep_out(int key, int octave, int vol)
{
    // 音波の周期
    uint16_t cycle = KEY_COUNT[key];
    if(octave == 1){
        cycle /= 2; // 1オクターブ上げる
    }else if(octave == -1){
        cycle *= 2; // 1オクターブ下げる
    }
    
    // 音波のデューティー
    // 50%のとき最大音量
    uint16_t duty = (uint16_t)((uint32_t)cycle * (uint32_t)vol / 1024UL / 2UL);
    
    // MTU0に周期とデューティーを設定
    MTU0.TGRC = cycle;
    MTU0.TGRD = duty;
}

// 初期化
void setup()
{
    // STチャンネルのキー判定閾値の算出
    for(int i = 0; i < 6; i++){
        FRET[i] = PW_MAX - (2*i + 1) * (PW_MAX - PW_MIN) / 12;
    }
    // キーのメータ表示のための角度定数
    for(int i = 0; i < 7; i++){
        POS_KEY[i] = POS_KEY_C + ((POS_KEY_B - POS_KEY_C) * i) / 6;
    }
    
    // シリアルポートの初期化(デバッグ用)
    Serial.begin(115200);
    
    // パルス幅計測器の初期化
    pwmeterST.begin  (PIN_SH);
    pwmeterTH.begin  (PIN_TH);
    pwmeterAUX1.begin(PIN_AUX1);
    pwmeterAUX2.begin(PIN_AUX2);
    
    // ビープ音の初期化
    beep_init();
    
    // メータ用のサーボの初期化
    servoOctave.attach(PIN_OCTAVE);
    servoKey.attach(PIN_KEY);
    servoVol.attach(PIN_VOL);
    servoOctave.write(POS_OCT4);
    servoKey.write(POS_KEY[3]);
    servoVol.write(POS_VOL0);
    
    // 受信表示LEDの初期化
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
}

// メインループ
void loop()
{
    int16_t pos;
    
    // 各チャンネルのパルス幅取得
    static uint16_t st = PW_NEU;
    static uint16_t th = TH_NEU;
    static uint16_t aux1 = PW_NEU;
    static uint16_t aux2 = PW_NEU;
    static int no_data_cnt = 0;
    int cnt = 0;
    if(pwmeterST.available()){
        st = pwmeterST.getLast();
        cnt++;
    }
    if(pwmeterTH.available()){
        th = pwmeterTH.getLast();
        cnt++;
    }
    if(pwmeterAUX1.available()){
        aux1 = pwmeterAUX1.getLast();
        cnt++;
    }
    if(pwmeterAUX2.available()){
        aux2 = pwmeterAUX2.getLast();
        cnt++;
    }
    if(cnt == 0){
        no_data_cnt++;
        if(no_data_cnt > 10){
            receiving = false;
            no_data_cnt = 0;
        }
    }else{
        receiving = true;
        no_data_cnt = 0;
    }
    
#if 1
    // パルス幅確認
    Serial.print("ST:");    Serial.print(st);   Serial.print("  ");
    Serial.print("TH:");    Serial.print(th);   Serial.print("  ");
    Serial.print("AUX1:");  Serial.print(aux1); Serial.print("  ");
    Serial.print("AUX2:");  Serial.print(aux2); Serial.print("\n");
#endif

    // キー判定 (STチャンネル)
    int key = KEY_B;
    int key2 = 6; // メータ表示用
    for(int i = 0; i < 6; i++){
        if(st > FRET[i]){
            key = SCALE[i];
            key2 = i;
            break;
        }
    }
    
    // オクターブ切り替え (AUX1チャンネル)
    int octave = 0;
    pos = (int16_t)(aux1 - PW_NEU);
    if(pos > PW_AUX){
        octave = -1;
    }else if(pos < -PW_AUX){
        octave = +1;
    }
#if 1
    // AUX2チャンネルもオクターブ切り替えに使う (操作性の都合)
    pos = (int16_t)(aux2 - PW_NEU);
    if(pos > PW_AUX){
        octave = -1;
    }else if(pos < -PW_AUX){
        octave = +1;
    }
    
#else
    // 半音上げ下げ (AUX2チャンネル)
    pos = (int16_t)(aux2 - PW_NEU);
    if(pos > PW_AUX){
        if((octave == 1) && (key == KEY_B)){
            // B5より上は無効
        }else{
            key++; // 半音上げる
            if(key > KEY_B){
                key = KEY_C; // Bの半音上は上のオクタープのC
                octave++;
            }
        }
    }else if(pos < -PW_AUX){
        if((octave == -1) && (key == KEY_C)){
            // C3より下は無効
        }else{
            key--; // 半音下げる
            if(key < KEY_C){
                key = KEY_B; // Cの半音下は下のオクタープのB
                octave--;
            }
        }
    }
#endif
    
    // 音量 (THチャンネル)
    pos = (int16_t)(th - TH_NEU);
    if(pos < TH_PLAY) pos=0;
    int vol = (int)pos * 1024 / TH_AMP;
    if(vol > 1024) vol =1024;
    // 音量を指数曲線に変換
    int n = vol / 128; // n = 0,1...8
    int m = vol % 128; // m = 0,1...127
    if(n < 8){
        vol = VOL_TABLE[n] + ((VOL_TABLE[n+1] - VOL_TABLE[n]) * m) / 128;
    }else{
        vol = 1024;
    }

#if 0
    // キー、オクターブ、音量の確認
    Serial.print("key:");     Serial.print(key);    Serial.print("\t");
    Serial.print("octave:");  Serial.print(octave); Serial.print("\t");
    Serial.print("vol:");     Serial.print(vol);    Serial.print("\n");
#endif

    // ビープ音出力
    beep_out(key, octave, vol);
    
    // オクターブのメータ表示
    servoOctave.write(POS_OCTAVE[octave + 1]);
    // キーのメータ表示
    servoKey.write(POS_KEY[key2]);
    // 音量のメータ表示
    int vol_pos = POS_VOL0 + ((POS_VOL100 - POS_VOL0) * vol) / 1024;
    servoVol.write(vol_pos);
    
    // 受信表示LED
    if(receiving){
        digitalWrite(PIN_LED, LOW);
    }else{
        digitalWrite(PIN_LED, HIGH);
    }
    
    delay(10);
}

