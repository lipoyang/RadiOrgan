// このスケッチはGR-CITRUS専用です。
#include "PWMeter4Citrus.h"
#include "SimpleYMF825.h"
#include "Servo.h"

// ピン番号定数
#define PIN_SH          0  // STチャンネル入力
#define PIN_TH          1  // THチャンネル入力
#define PIN_AUX1        2  // AUX1チャンネル入力
#define PIN_AUX2        3  // AUX2チャンネル入力

#define PIN_OCTAVE      8  // オクターブ表示出力
#define PIN_KEY         7  // キー表示出力
#define PIN_VOL         5  // 音量表示出力

#define PIN_LED         6  // 受信表示LED出力

#define PIN_VOLUME      14 // ボリューム入力
#define PIN_TONE        15 // 音色切り替え入力
#define PIN_SCALE       16 // 音階切り替え入力

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

// 7音音階番号
#define KEY7_C          0
#define KEY7_D          1
#define KEY7_E          2
#define KEY7_F          3
#define KEY7_G          4
#define KEY7_A          5
#define KEY7_B          6

// 打鍵状態
#define KEY_OFF         0
#define KEY_ON1         1
#define KEY_ON2         2

// メータ表示用サーボの角度定数 (実機に合わせて微調整)
#define POS_OCT3    (120-2) // オクターブ3
#define POS_OCT4    (90-2)  // オクターブ4
#define POS_OCT5    (60-2)  // オクターブ5
#define POS_KEY_C   150     // キー:C
#define POS_KEY_B   35      // キー:B
#define POS_VOL0    130     // 音量ゼロ
#define POS_VOL100  25      // 音量最大

// 音階 (ハ長調) テーブル
int SCALE[7] = {KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_A, KEY_B};

// STチャンネルのキー判定閾値テーブル
// ギターのフレットのようなもの。これにより離散的な音階になる。
// (実行時にsetup()で計算)
uint16_t FRET[6];

// 各オクターブの表示角度
const int POS_OCTAVE[3] = {POS_OCT3, POS_OCT4, POS_OCT5};
// 各キーの表示角度 (実行時にsetup()で計算)
int POS_KEY[7];

// 調号テーブル (-1:♭ +1:#)
const int KEY_TABLE[13][7] = {
//    C   D   E   F   G   A   B
    {-1, -1, -1,  0, -1, -1, -1}, // [0]:G♭ (変ト長調)
    { 0, -1, -1,  0, -1, -1, -1}, // [1]:D♭ (変ニ長調)
    { 0, -1, -1,  0,  0, -1, -1}, // [2]:A♭ (変イ長調)
    { 0,  0, -1,  0,  0, -1, -1}, // [3]:E♭ (変ホ長調)
    { 0,  0, -1,  0,  0,  0, -1}, // [4]:B♭ (変ロ長調)
    { 0,  0,  0,  0,  0,  0, -1}, // [5]:F (ヘ長調)
    { 0,  0,  0,  0,  0,  0,  0}, // [6]:C (ハ長調)
    { 0,  0,  0, +1,  0,  0,  0}, // [7]:G (ト長調)
    {+1,  0,  0, +1,  0,  0,  0}, // [8]:D (ニ長調)
    {+1,  0,  0, +1, +1,  0,  0}, // [9]:A (イ長調)
    {+1, +1,  0, +1, +1,  0,  0}, // [10]:E (ホ長調)
    {+1, +1,  0, +1, +1, +1,  0}, // [11]:B (ロ長調)
    {+1, +1, +1, +1, +1, +1,  0}, // [12]:F# (嬰ヘ長調)
};

// 音色テーブル
const int TONE_TABLE[8] = {
    // 弦楽器系
    GRAND_PIANO,
    TNKL_BELL,
    NYLON_GUITER,
    HARPSICHORD,
    // 管楽器系
    CHURCH_ORGAN,
    FLUTE,
    ROCK_ORGAN,
    HARMONICA
};

// FM音源
SimpleYMF825 ymf825;

// パルス幅計測器
PWMeter pwmeterST;      // STチャンネル
PWMeter pwmeterTH;      // THチャンネル
PWMeter pwmeterAUX1;    // AUX1チャンネル
PWMeter pwmeterAUX2;    // AUX2チャンネル

// メータ表示用サーボ
Servo servoOctave;  // オクターブ
Servo servoKey;     // キー
Servo servoVol;     // 音量

// 変数
bool receiving = false;     // 受信中か？
int key_state = KEY_OFF;    // 打鍵状態
int master_vol = -1;// マスター音量
int tone_no = -1;    // 音色
int scale;          // 音階(何調か、つまりどこに#や♭が付くか)

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
    
    // メータ用のサーボの初期化
    servoOctave.attach(PIN_OCTAVE);
    servoKey.attach(PIN_KEY);
    servoVol.attach(PIN_VOL);
    servoOctave.write(POS_OCT4);
    servoKey.write(POS_KEY[KEY7_F]);
    servoVol.write(POS_VOL0);
    
    // 受信表示LEDの初期化
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
    
    // FM音源の初期化 (チャンネルに音色を割り当て)
    ymf825.begin(IOVDD_3V3);
}

// メインループ
void loop()
{
    // マスター音量
    int a_vol = analogRead(PIN_VOLUME);
    int temp_vol = 32 + (a_vol * 32) / 675;
    if (temp_vol > 63) temp_vol = 63;
    if(master_vol != temp_vol){
        master_vol = temp_vol;
        ymf825.setMasterVolume(master_vol);
    }
    
    // 音色
    int a_tone = analogRead(PIN_TONE);
    int tone_temp = (a_tone * 8) / 675;
    if(tone_temp > 7) tone_temp = 7;
    if(tone_no != tone_temp){
        tone_no = tone_temp;
        ymf825.setTone( 0, TONE_TABLE[tone_no] );
    }
    Serial.println(tone_no);
    
    // 音階
    int a_scale = analogRead(PIN_SCALE);
    scale = (a_scale * 14) / 675;
    if(scale > 13) scale = 13;
    
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
    
#if 0
    // パルス幅確認
    Serial.print("ST:");    Serial.print(st);   Serial.print("  ");
    Serial.print("TH:");    Serial.print(th);   Serial.print("  ");
    Serial.print("AUX1:");  Serial.print(aux1); Serial.print("  ");
    Serial.print("AUX2:");  Serial.print(aux2); Serial.print("\n");
#endif

    // キー判定 (STチャンネル)
    int key12 = KEY_B;  // 12音音階番号
    int key7  = KEY7_B; // 7音音階番号
    for(int i = KEY7_C; i < KEY7_B; i++){
        if(st > FRET[i]){
            key12 = SCALE[i];
            key7 = i;
            break;
        }
    }
    
    // 相対オクターブ
    int r_octave = 0; 
    // AUX1チャンネルでオクターブ切り替え
    pos = (int16_t)(aux1 - PW_NEU);
    if(pos > PW_AUX){
        r_octave = -1;
    }else if(pos < -PW_AUX){
        r_octave = +1;
    }
    // AUX2チャンネルもオクターブ切り替えに使う (操作性の都合)
    pos = (int16_t)(aux2 - PW_NEU);
    if(pos > PW_AUX){
        r_octave = -1;
    }else if(pos < -PW_AUX){
        r_octave = +1;
    }
    // 絶対オクターブ
    int octave = 4 + r_octave;
    
    // 調号
/*
    int key_sign = KEY_TABLE[scale][key7];
    key12 += key_sign;
    if(key12 > KEY_B){
        key12 = KEY_C;
        octave++;
    }
    else if(key12 < KEY_C){
        key12 = KEY_B;
        octave--;
    }
*/

    
    // 音量 (THチャンネル)
    pos = (int16_t)(th - TH_NEU);
    if(pos < TH_PLAY) pos=0;
    int vol = (int)pos * 31 / TH_AMP;
    if(vol > 31) vol = 31;
    
    // サウンド出力
    // 管楽器系
    if(tone_no >= 4)
    {
        if(vol > 0){
            ymf825.keyon(0, octave, key12, vol);
        }else{
            ymf825.keyoff(0);
        }
    }
    // 弦楽器系
    else
    {
        static int vol_old = 0;
        static int keyon_cnt = 0;
        bool is_keyon = false;
        switch(key_state){
            case KEY_OFF:
                if(vol > 0){
                    key_state = KEY_ON1;
                    vol_old = vol;
                    keyon_cnt = 0;
                }
                break;
            case KEY_ON1:
                keyon_cnt++;
                if(keyon_cnt > 20){
                    is_keyon = true;
                }else{
                    if(vol >= vol_old){
                        vol_old = vol;
                    }else{
                        is_keyon = true;
                    }
                }
                if(is_keyon){
                    ymf825.keyon(0, octave, key12, vol_old);
                    key_state = KEY_ON2;
                }
                break;
            case KEY_ON2:
                if(pos < TH_PLAY){
                    key_state = KEY_OFF;
                    ymf825.keyoff(0);
                }
                break;
        }
    }
    
#if 0
    // キー、オクターブ、音量の確認
    Serial.print("tone_no:"); Serial.print(tone_no);Serial.print("\t");
    Serial.print("key12:");   Serial.print(key12);  Serial.print("\t");
    Serial.print("octave:");  Serial.print(octave); Serial.print("\t");
    Serial.print("vol:");     Serial.print(vol);    Serial.print("\n");
#endif
    
    // オクターブのメータ表示
    servoOctave.write(POS_OCTAVE[r_octave + 1]);
    // キーのメータ表示
    servoKey.write(POS_KEY[key7]);
    // 音量のメータ表示
    int vol_pos = POS_VOL0 + ((POS_VOL100 - POS_VOL0) * pos) / TH_AMP;
    servoVol.write(vol_pos);
    
    // 受信表示LED
    if(receiving){
        digitalWrite(PIN_LED, LOW);
    }else{
        digitalWrite(PIN_LED, HIGH);
    }
    
    delay(5);
}

