// このスケッチはGR-CITRUS専用です。
#include "PWMeter4Citrus.h"
#include "SimpleYMF825.h"
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
#define PIN_SH          0  // STチャンネル入力
#define PIN_TH          1  // THチャンネル入力
#define PIN_AUX1        2  // AUX1チャンネル入力
#define PIN_AUX2        3  // AUX2チャンネル入力

#define PIN_OCTAVE      5  // オクターブ表示出力
#define PIN_KEY         7  // キー表示出力
#define PIN_VOL         8  // 音量表示出力

#define PIN_LED         6  // 受信表示LED出力

#define PIN_VOLUME      14 // ボリューム入力
#define PIN_TONE        15 // 音色切り替え入力
#define PIN_SCALE       16 // 音階切り替え入力

// 7音音階番号
#define KEY7_C          0
#define KEY7_D          1
#define KEY7_E          2
#define KEY7_F          3
#define KEY7_G          4
#define KEY7_A          5
#define KEY7_B          6

// FM音源
SimpleYMF825 ymf825;

// パルス幅計測器
PWMeter pwmeterST;      // STチャンネル
PWMeter pwmeterTH;      // THチャンネル
PWMeter pwmeterAUX1;    // AUX1チャンネル
PWMeter pwmeterAUX2;    // AUX2チャンネル

// 音階 (ハ長調) テーブル
int SCALE[7] = {KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_A, KEY_B};

// STチャンネルのキー判定閾値テーブル
// ギターのフレットのようなもの。これにより離散的な音階になる。
// (実行時にsetup()で計算)
uint16_t FRET[6];

// メータ表示用サーボの角度定数 (実機に合わせて微調整)
#define POS_OCT3    (120-2) // オクターブ3
#define POS_OCT4    (90-2)  // オクターブ4
#define POS_OCT5    (60-2)  // オクターブ5
#define POS_KEY_C   150     // キー:C
#define POS_KEY_B   35      // キー:B
#define POS_VOL0    130     // 音量ゼロ
#define POS_VOL100  25      // 音量最大

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

// メータ表示用サーボ
Servo servoOctave;  // オクターブ
Servo servoKey;     // キー
Servo servoVol;     // 音量

// 受信中か？
bool receiving = false;

int master_vol;     // マスター音量
int ch_tone;        // 音色(チャンネルと音色を一対一に割り当て)
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
    
    // FM音源の初期化 (チャンネルと音色を一対一に割り当て)
    ymf825.begin(IOVDD_3V3);
    ymf825.setTone( 0, GRAND_PIANO );
    ymf825.setTone( 1, E_PIANO );
    ymf825.setTone( 2, TENOR_SAX );
    ymf825.setTone( 3, PICK_BASS );
    ymf825.setTone( 4, TNKL_BELL );
    ymf825.setTone( 5, NEW_AGE_PD );
    ymf825.setTone( 6, BRIGHT_PIANO );
    ymf825.setTone( 7, VIBES );
    ymf825.setTone( 8, CHURCH_ORGAN );
    ymf825.setTone( 9, FLUTE );
    ymf825.setTone(10, ROCK_ORGAN );
    ymf825.setTone(11, NYLON_GUITER );
    ymf825.setTone(12, SQUARE_LEAD );
    ymf825.setTone(13, SAW_LEAD );
    ymf825.setTone(14, HARPSICHORD );
    ymf825.setTone(15, HARMONICA );
    // TODO
    //for(int ch=0;ch<16;ch++){
    //    YMF825.setVolume(ch, 31);
    //}
    
    // マスター音量
    int a_vol = analogRead(PIN_VOLUME);
    master_vol = (a_vol * 64) / 675;
    if (master_vol > 63) master_vol = 63;
    ymf825.setMasterVolume(master_vol);
}

// メインループ
void loop()
{
    // マスター音量
    int a_vol = analogRead(PIN_VOLUME);
    int temp_vol = (a_vol * 64) / 675;
    if (temp_vol > 63) temp_vol = 63;
    if(master_vol != temp_vol){
        master_vol = temp_vol;
        ymf825.setMasterVolume(master_vol);
    }
    
    // 音色
    int a_tone = analogRead(PIN_TONE);
    ch_tone = (a_tone * 16) / 675;
    if(ch_tone > 15) ch_tone = 15;
    
    // 音階 TODO
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
    
#if 1
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
    
    // 音量 (THチャンネル)
    pos = (int16_t)(th - TH_NEU);
    if(pos < TH_PLAY) pos=0;
    int vol = (int)pos * 31 / TH_AMP;
    if(vol > 31) vol = 31;
    
    // 調号
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
    
#if 0
    // キー、オクターブ、音量の確認
    Serial.print("key12:");   Serial.print(key12);  Serial.print("\t");
    Serial.print("octave:");  Serial.print(octave); Serial.print("\t");
    Serial.print("vol:");     Serial.print(vol);    Serial.print("\n");
#endif

    // サウンド出力
    if(vol >= 0){
        ymf825.keyon(ch_tone, octave, key12, vol);
    }else{
        ymf825.keyoff(ch_tone);
    }
    // オクターブのメータ表示
    servoOctave.write(POS_OCTAVE[r_octave + 1]);
    // キーのメータ表示
    servoKey.write(POS_KEY[key7]);
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

