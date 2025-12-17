# UIAP KEYER for ch32fun 

UIAP KEYERは蚰蜒倶楽部さんが2025年12月7日の電波文化祭5で配布されたUIAPduinoを使用したモールスキーヤーです。  
Arduinoを使用したスケッチが公開されていますが、今回オブジェクト効率が高いch32funベースに移植しました。

[UIAP KEYER販売サイト](https://www.gejigeji.com/?page_id=1045)  
[ch32vfunサポートサイト](https://github.com/cnlohr/ch32fun)  

## 開発環境
ビルド環境はVSCode + PlatformIO + ch32v003funが必要です。  
書き込は、WCH-LinkEまたは、UIAPのUSB端子から行います。

## サンプルコード
### uiap_keyer_for_ch32fun
UIAP KEYERをch32fun環境に移植したものです。ハードウェアや開発環境に依存した部分をkeyer_hal.cppにまとめてあります。

### ui2c_oled
UIAP KEYERには4ピンのI2C OLEDを搭載可能です。  
ch32funに含まれるSSD1306 OLEDライブラリにカナフォントを追加したものをlibフォルダに格納してあります。

### uiap_message_keyer_for_ch32fun
uiap_message_keyer_for_ch32funに固定メッセージ送信機能を追加したものです。メッセージはOLEDに表示されます。  

■ 変更履歴


■ ライセンス

