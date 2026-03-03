# ESP32-OTA

ESP32向けのOTA（Over-The-Air）ファームウェア更新が可能なWebサーバーアプリケーション。

## 機能

- **WiFi接続管理**
  - DHCP / 静的IP設定に対応
  - SPIFFSへの設定保存
  - 接続エラー時の自動APモード

- **OTAファームウェア更新**
  - ArduinoOTAによるWiFi経由の更新
  - HTTP POSTによるファームウェア更新

- **Syslogロギング**
  - Syslogサーバーへのログ転送
  - ファシリティ・アプリケーション名の設定

- **Webサーバー**
  - React + TypeScript製の管理画面
  - WiFi設定の変更
  - システム情報の表示
  - Syslog設定の管理

- **その他**
  - NTP同期（ntp.nict.jp）
  - WiFi設定リセット（GPIO 18）

## ハードウェア要件

- ESP32開発ボード
- GPIO 18: WiFi設定リセット用ボタン

## 依存関係

### PlatformIOライブラリ

platformio.iniを参照してください。

### フロントエンド

- React 19
- TypeScript
- Vite
- TailwindCSS

## セットアップ

### 1. ファームウェアのビルドとアップロード

```bash
# プロジェクトディレクトリへ移動
cd ESP32-OTA

# ビルドとアップロード
pio run --target upload

# シリアルモニターでログを確認
pio device monitor
```

### 2. WiFi設定

初回起動時、`wifi.json` が存在しない場合、APモードで起動します。

- SSID: `ESP32-OTA`
- パスワード: なし
- IPアドレス: `192.168.0.1`

ブラウザで `http://192.168.0.1` にアクセスし、WiFi設定を行います。

### 3. フロントエンドの開発

```bash
cd frontend

# 依存関係のインストール
npm install

# 開発サーバーの開始
npm run dev

# プロダクションビルド
npm run build
```

### 4. SPIFFSデータのアップロード

```bash
# dataディレクトリ内のファイルをアップロード
# WiFi設定（data/wifi.json）とフロントエンド（data/web/）が含まれます
pio run --target uploadfs
```

## 設定ファイル

### wifi.json

SPIFFSのルートディレクトリに配置するWiFi設定ファイルです。
ゲートウェイ設定も必要です（`nameservers`と`gateway`は別フィールド）。

```json
{
  "ssid": "your_wifi_ssid",
  "password": "your_wifi_password",
  "hostname": "ESP32-OTA",
  "type": "dhcp",
  "ipaddress": "",
  "gateway": "",
  "netmask": "",
  "nameservers": []
}
```

静的IP設定の場合（`type: "static"`）:

```json
{
  "ssid": "your_wifi_ssid",
  "password": "your_wifi_password",
  "hostname": "ESP32-OTA",
  "type": "static",
  "ipaddress": "192.168.1.100",
  "gateway": "192.168.1.1",
  "netmask": "255.255.255.0",
  "nameservers": ["8.8.8.8", "8.8.4.4"]
}
```

### syslog.json

Syslogサーバーの設定ファイルです。

```json
{
  "enabled": true,
  "server": "192.168.1.10",
  "port": 514,
  "facility": "user"
}
```

## APIエンドポイント

| エンドポイント | メソッド | 説明 |
|-------------|---------|------|
| `/` | GET | 管理画面 |
| `/api/status` | GET | システム情報の取得（IP、ファームウェアバージョン、システム時間など） |
| `/api/config` | GET | 設定全体の取得（WiFi、hostname、OTA URL、Syslog、NTP） |
| `/api/config` | POST | 設定全体の更新（自動再起動） |
| `/api/wifi` | POST | WiFi設定の更新（自動再起動） |
| `/api/syslog` | POST | Syslog設定の更新 |
| `/api/ntp` | POST | NTPサーバー設定の更新 |
| `/api/ota-update` | POST | OTAファームウェア更新（OTA URLをJSONで送信） |

## OTA更新方法

### ArduinoOTAを使用する場合

1. `platformio.ini` のコメントを解除:
   ```
   upload_protocol = espota
   upload_port = 192.168.x.x
   ```
2. 更新コマンドを実行:
   ```bash
   pio run --target upload
   ```

### HTTP POSTを使用する場合

OTA URLを指定してファームウェアを更新します。

1. OTA URLをJSONで送信:
   ```bash
   curl -X POST -H "Content-Type: application/json" -d '{"ota_url":"http://your-server/firmware.bin"}' http://esp32-ip/api/ota-update
   ```
2. 更新中はシステム情報を `/api/status` で確認できます

## WiFiのリセット

GPIO 18をGNDに接続すると、WiFi設定がリセットされ、APモードで再起動します。設定ファイルは削除されますのでご注意ください。

## ファームウェアバージョン

ビルド時に `[env:esp32dev] build_flags = -DFIRMWARE_VERSION=x.y.z` でカスタマイズ可能です。

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。
