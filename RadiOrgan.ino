// このスケッチはGR-CITRUS専用です。
#include "PWMeter4Citrus.h"
#include "SimpleYMF825.h"
#include "Servo.h"

#include "RadiOrgan.h"

// FM音源
SimpleYMF825 ymf825;

// パルス幅計測器
PWMeter pwmeterST;      // STチャンネル
PWMeter pwmeterTH;      // THチャンネル
PWMeter pwmeterAUX1;    // AUX1チャンネル
PWMeter pwmeterAUX2;    // AUX2チャンネル

// メータ表示用サーボ
Servo servoOctave;      // オクターブ
Servo servoKey;         // キー
Servo servoVol;         // 音量

// パルス幅
uint16_t st   = PW_NEU; // STチャンネル
uint16_t th   = TH_NEU; // THチャンネル
uint16_t aux1 = PW_NEU; // AUX1チャンネル
uint16_t aux2 = PW_NEU; // AUX2チャンネル

bool receiving = false; // 受信中か？
int key_state = KEY_OFF;// 打鍵状態
int master_vol = -1;    // マスター音量
int tone_no = -1;       // 音色
int scale;              // 音階(何調か、つまりどこに#や♭が付くか)

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
    // アナログ入力の取得と処理
    analog_input();
    
    // パルス幅の取得
    pulse_input();
    
    // サウンドの計算
    int key12;       // 12音音階番号
    int key7;        // 7音音階番号
    int r_octave;    // 相対オクターブ
    int octave;      // 絶対オクターブ
    int vol;         // 音量
    int16_t pw_vol;  // 音量(パルス指令値)
    sound_calc(key12, key7, r_octave, octave, vol, pw_vol);
    
    // サウンド出力
    sound_output(octave, key12, vol);
    
    // 表示出力
    display_output(r_octave, key7, pw_vol);
    
    delay(5);
}

// アナログ入力の取得と処理
void analog_input()
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
    
    // 音階
    int a_scale = analogRead(PIN_SCALE);
    scale = (a_scale * 14) / 675;
    if(scale > 13) scale = 13;
}

// パルス幅の取得
void pulse_input()
{
    // 各チャンネルのパルス幅取得
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
}

// サウンドの計算
void sound_calc(int &key12, int &key7, int &r_octave, int &octave, int &vol, int16_t &pw_vol)
{
    // キー判定 (STチャンネル)
    key12 = KEY_B;
    key7  = KEY7_B;
    for(int i = KEY7_C; i < KEY7_B; i++){
        if(st > FRET[i]){
            key12 = SCALE[i];
            key7 = i;
            break;
        }
    }
    
    // オクターブ判定 (AUXチャンネル)
    r_octave = 0; 
    // AUX1
    int16_t pw = (int16_t)(aux1 - PW_NEU);
    if(pw > PW_AUX){
        r_octave = -1;
    }else if(pw < -PW_AUX){
        r_octave = +1;
    }
    // AUX2
    pw = (int16_t)(aux2 - PW_NEU);
    if(pw > PW_AUX){
        r_octave = -1;
    }else if(pw < -PW_AUX){
        r_octave = +1;
    }
    octave = 4 + r_octave;
    
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
    pw = (int16_t)(th - TH_NEU);
    if(pw < TH_PLAY) pw=0;
    vol = (int)pw * 31 / TH_AMP;
    if(vol > 31) vol = 31;
    pw_vol = pw;
}

// サウンド出力
void sound_output(int octave, int key12, int vol)
{
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
                if(vol == 0){
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
}

// 表示出力
void display_output(int r_octave, int key7, int16_t pw_vol)
{
    // オクターブのメータ表示
    servoOctave.write(POS_OCTAVE[r_octave + 1]);
    // キーのメータ表示
    servoKey.write(POS_KEY[key7]);
    // 音量のメータ表示
    int pos_vol = POS_VOL0 + ((POS_VOL100 - POS_VOL0) * pw_vol) / TH_AMP;
    servoVol.write(pos_vol);
    
    // 受信表示LED
    if(receiving){
        digitalWrite(PIN_LED, LOW);
    }else{
        digitalWrite(PIN_LED, HIGH);
    }
}
