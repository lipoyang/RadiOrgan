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
#define TH_AMP_H     480 // 振幅 (アクセル側)
#define TH_AMP_B     310 // 振幅 (ブレーキ側)
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

// 弦楽器の最小音量閾値
#define STRINGS_MIN     3

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

