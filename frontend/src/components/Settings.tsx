import React, { useState, useEffect } from 'react';

const Settings: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'wifi' | 'firmware-config' | 'syslog' | 'ntp'>('wifi');
  const [type, setType] = useState<string>('static');
  const [hostname, setHostname] = useState<string>('');
  const [ssid, setSsid] = useState<string>('');
  const [password, setPassword] = useState<string>('');
  const [ipAddress, setIpAddress] = useState<string>('');
  const [gateway, setGateway] = useState<string>('');
  const [subnetMask, setSubnetMask] = useState<string>('');
  const [dns1, setDns1] = useState<string>('');
  const [dns2, setDns2] = useState<string>('');
  const [otaUrl, setOtaUrl] = useState<string>('');
  const [result, setResult] = useState<string>('');
  const [file, setFile] = useState<File | null>(null);
  const [otaLoading, setOtaLoading] = useState<boolean>(false);
  const [syslogServer, setSyslogServer] = useState<string>('');
  const [syslogPort, setSyslogPort] = useState<string>('514');
  const [syslogFacility, setSyslogFacility] = useState<string>('user');
  const [syslogAppName, setSyslogAppName] = useState<string>('OTA');
  const [syslogEnabled, setSyslogEnabled] = useState<boolean>(false);
  const [ntpServer, setNtpServer] = useState<string>('ntp.nict.jp');
  const [loading, setLoading] = useState<boolean>(false);

  useEffect(() => {
    const fetchConfig = async () => {
      try {
        const response = await fetch('/api/config');
        if (response.ok) {
          const data = await response.json();
          const wifi = data.wifi || {};

          setType(wifi.type || 'static');
          setHostname(data.hostname || 'ESP32-OTA');
          setSsid(wifi.ssid || '');
          setPassword(wifi.password || '');
          setIpAddress(wifi.ipaddress || '');
          setGateway(wifi.gateway || '');
          setSubnetMask(wifi.netmask || '');
          setDns1(wifi.nameservers?.[0] || '');
          setDns2(wifi.nameservers?.[1] || '');
          setOtaUrl(data.ota_url || '');
          const syslog = data.syslog || {};
          setSyslogServer(syslog.server || '');
          setSyslogPort(syslog.port || '514');
          setSyslogFacility(syslog.facility || 'user');
          setSyslogAppName(syslog.appname || 'OTA');
          setSyslogEnabled(syslog.enabled || false);
          setNtpServer(data.ntp_server || 'ntp.nict.jp');
        } else {
          setResult('設定情報の取得に失敗しました。');
        }
      } catch (error) {
        setResult('エラーが発生しました。');
      }
    };

    fetchConfig();
  }, []);

  const handleSubmit = async (e: React.SyntheticEvent<HTMLFormElement>) => {
    e.preventDefault();

    // IPアドレスの検証
    if (type === 'static') {
      if (ipAddress && !isValidIP(ipAddress)) {
        setResult('IPアドレスの形式が正しくありません。');
        return;
      }
      if (gateway && !isValidIP(gateway)) {
        setResult('ゲートウェイの形式が正しくありません。');
        return;
      }
      if (subnetMask && !isValidIP(subnetMask)) {
        setResult('サブネットマスクの形式が正しくありません。');
        return;
      }
      if (dns1 && !isValidIP(dns1)) {
        setResult('DNSサーバー1の形式が正しくありません。');
        return;
      }
      if (dns2 && !isValidIP(dns2)) {
        setResult('DNSサーバー2の形式が正しくありません。');
        return;
      }
    }

    try {
      const response = await fetchWithTimeout('/api/wifi', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          hostname,
          type,
          ssid,
          password,
          ...(type === 'static' && {
            ipaddress: ipAddress,
            gateway,
            netmask: subnetMask,
            nameservers: [dns1, dns2]
          }),
          ota_url: otaUrl
        }),
      });

      if (response.ok) {
        setLoading(true);
        setResult('設定が保存されました。再起動中（約10秒）...');
        setTimeout(() => {
          window.location.reload();
        }, 10000);
      } else {
        setResult('設定の保存に失敗しました。');
      }
    } catch (error) {
      setResult('エラーが発生しました。');
    }
  };

  const handleFirmwareUpdate = async () => {
    // OTA URLが設定されているか確認
    if (!otaUrl) {
      setResult('OTA URLを入力してください。');
      return;
    }

    try {
      setResult('ファームウェア更新を開始します...');
      const response = await fetchWithTimeout('/api/ota-update', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          ota_url: otaUrl
        }),
      }, 10000);  // 10秒タイムアウト

      if (response.ok) {
        const data = await response.json();
        console.log('OTA response:', data);
        const message = data.message || 'ファームウェア更新が開始されました。';
        setResult(message);
        console.log('Setting result:', message, 'Setting otaLoading to true');
        // 30秒ローディングを表示してから再起動
        setOtaLoading(true);
        console.log('otaLoading should now be true');
        setTimeout(() => {
          window.location.reload();
        }, 30000);
      } else {
        const errorData = await response.json();
        setResult(errorData.message || 'ファームウェア更新の開始に失敗しました。');
      }
    } catch (error) {
      console.error('OTA error:', error);
      setResult('エラーが発生しました。');
    }
  };

  const handleDownload = async () => {
    try {
      const response = await fetchWithTimeout('/api/config', {}, 10000);  // 10秒タイムアウト
      if (response.ok) {
        const blob = await response.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'config.json';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);
        setResult('設定ファイルをダウンロードしました。');
      } else {
        setResult('設定ファイルのダウンロードに失敗しました。');
      }
    } catch (error) {
      setResult('エラーが発生しました。');
    }
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (e.target.files && e.target.files.length > 0) {
      setFile(e.target.files[0]);
    }
  };

  const handleUpload = async () => {
    if (!file) {
      setResult('ファイルを選択してください。');
      return;
    }

    try {
      // ファイルの内容を読み取る
      const fileContent = await readFileAsText(file);

      // JSONとしてパース
      const jsonData = JSON.parse(fileContent);

      const response = await fetchWithTimeout('/api/config', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          ...jsonData,
          hostname: jsonData.hostname || hostname,
          ota_url: jsonData.ota_url || ''
        }),
      }, 10000);  // 10秒タイムアウト

      if (response.ok) {
        setLoading(true);
        setResult('設定ファイルをアップロードしました。再起動中（約10秒）...');
        setTimeout(() => {
          window.location.reload();
        }, 10000);
      } else {
        setResult('設定ファイルのアップロードに失敗しました。');
      }
    } catch (error) {
      setResult('エラーが発生しました。');
    }
  };

  // タイムアウト付きfetch
  const fetchWithTimeout = async (url: string, options: RequestInit = {}, timeout = 30000): Promise<Response> => {
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), timeout);
    try {
      const response = await fetch(url, { ...options, signal: controller.signal });
      clearTimeout(id);
      return response;
    } catch (error) {
      clearTimeout(id);
      throw error;
    }
  };

  // IPアドレス検証
  const isValidIP = (ip: string): boolean => {
    const ipRegex = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
    return ipRegex.test(ip);
  };

  // ファイルをテキストとして読み取るためのヘルパー関数
  const readFileAsText = (file: File): Promise<string> => {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(reader.result as string);
      reader.onerror = reject;
      reader.readAsText(file);
    });
  };

  const handleSyslogSubmit = async (e: React.SyntheticEvent<HTMLFormElement>) => {
    e.preventDefault();

    try {
      const response = await fetchWithTimeout('/api/syslog', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          hostname,
          enabled: syslogEnabled,
          server: syslogServer,
          port: syslogPort,
          facility: syslogFacility,
          appname: syslogAppName
        }),
      }, 10000);  // 10秒タイムアウト

      if (response.ok) {
        setResult('Syslog設定が保存されました。');
      } else {
        setResult('Syslog設定の保存に失敗しました。');
      }
    } catch (error) {
      setResult('エラーが発生しました。');
    }
  };

  const handleNtpSubmit = async (e: React.SyntheticEvent<HTMLFormElement>) => {
    e.preventDefault();

    try {
      const response = await fetchWithTimeout('/api/ntp', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          ntp_server: ntpServer
        }),
      }, 10000);  // 10秒タイムアウト

      if (response.ok) {
        setResult('NTP設定が保存され、適用されました。');
      } else {
        setResult('NTP設定の保存に失敗しました。');
      }
    } catch (error) {
      setResult('エラーが発生しました。');
    }
  };

  return (
    <div className="max-w-5xl mx-auto px-4 py-6 space-y-6">
      <header className="text-2xl font-semibold text-gray-800 border-b pb-2">
        ESP32 設定
      </header>

      {/* メインタブ */}
      <div className="border-b border-gray-200">
        <nav className="-mb-px flex space-x-8">
          <button
            onClick={() => setActiveTab('wifi')}
            className={`px-1 py-4 text-sm font-medium border-b-2 transition-colors ${activeTab === 'wifi'
                ? 'border-blue-500 text-blue-600'
                : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
          >
            Wi-Fi 設定
          </button>
          <button
            onClick={() => setActiveTab('firmware-config')}
            className={`px-1 py-4 text-sm font-medium border-b-2 transition-colors ${activeTab === 'firmware-config'
                ? 'border-blue-500 text-blue-600'
                : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
          >
            ファームウェアと設定ファイル
          </button>
          <button
            onClick={() => setActiveTab('syslog')}
            className={`px-1 py-4 text-sm font-medium border-b-2 transition-colors ${activeTab === 'syslog'
                ? 'border-blue-500 text-blue-600'
                : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
          >
            Syslog 設定
          </button>
          <button
            onClick={() => setActiveTab('ntp')}
            className={`px-1 py-4 text-sm font-medium border-b-2 transition-colors ${activeTab === 'ntp'
                ? 'border-blue-500 text-blue-600'
                : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
          >
            NTP 設定
          </button>
        </nav>
      </div>

      {/* Wi-Fi 設定タブ */}
      {activeTab === 'wifi' && (
        <section className="bg-white border rounded shadow p-6 space-y-4">
          <h2 className="text-lg font-bold text-gray-700">Wi-Fi 設定</h2>
          <form onSubmit={handleSubmit} className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2 text-left">接続タイプ</label>
              <div className="flex space-x-6">
                <label className="flex items-center">
                  <input
                    type="radio"
                    name="connectionType"
                    value="static"
                    checked={type === 'static'}
                    onChange={(e) => setType(e.target.value)}
                    className="mr-2"
                  />
                  <span>Static</span>
                </label>
                <label className="flex items-center">
                  <input
                    type="radio"
                    name="connectionType"
                    value="dhcp"
                    checked={type === 'dhcp'}
                    onChange={(e) => setType(e.target.value)}
                    className="mr-2"
                  />
                  <span>DHCP</span>
                </label>
              </div>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 text-left">ホスト名</label>
              <input
                type="text"
                value={hostname}
                onChange={(e) => setHostname(e.target.value)}
                className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                placeholder="ESP32-OTA"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 text-left">SSID</label>
              <input
                type="text"
                value={ssid}
                onChange={(e) => setSsid(e.target.value)}
                className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                required
              />
            </div>
            <div>
              <label className="block text-sm font-medium text-gray-700 text-left">パスワード</label>
              <input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                required
              />
            </div>

            {type === 'static' && (
              <>
                <div>
                  <label className="block text-sm font-medium text-gray-700 text-left">IPアドレス</label>
                  <input
                    type="text"
                    value={ipAddress}
                    onChange={(e) => setIpAddress(e.target.value)}
                    className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                    required
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 text-left">ゲートウェイ</label>
                  <input
                    type="text"
                    value={gateway}
                    onChange={(e) => setGateway(e.target.value)}
                    className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                    required
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 text-left">サブネットマスク</label>
                  <input
                    type="text"
                    value={subnetMask}
                    onChange={(e) => setSubnetMask(e.target.value)}
                    className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                    required
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 text-left">DNSサーバー1</label>
                  <input
                    type="text"
                    value={dns1}
                    onChange={(e) => setDns1(e.target.value)}
                    className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                    required
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 text-left">DNSサーバー2</label>
                  <input
                    type="text"
                    value={dns2}
                    onChange={(e) => setDns2(e.target.value)}
                    className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                    required
                  />
                </div>
              </>
            )}

            <button
              type="submit"
              className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 transition"
            >
              設定を保存
            </button>
          </form>
        </section>
      )}

      {/* システム設定タブ */}
      {activeTab === 'firmware-config' && (
        <div className="space-y-6">
          {/* OTA更新 */}
          <section className="bg-white border rounded shadow p-6 space-y-4">
            <h2 className="text-lg font-bold text-gray-700">OTA更新</h2>
            <div className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 text-left">OTA URL</label>
                <input
                  type="text"
                  value={otaUrl}
                  onChange={(e) => setOtaUrl(e.target.value)}
                  className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                  placeholder="http://example.com/firmware.bin"
                />
              </div>
              <div className="flex space-x-4">
                <button
                  onClick={handleFirmwareUpdate}
                  disabled={!otaUrl}
                  className={`px-4 py-2 rounded transition ${otaUrl
                      ? "bg-green-600 text-white hover:bg-green-700"
                      : "bg-gray-300 text-gray-500 cursor-not-allowed"
                    }`}
                >
                  ファームウェア更新
                </button>
              </div>
              <p className="text-sm text-gray-500">
                OTA URLを入力して「設定を保存」ボタンをクリックすると、次の起動時に指定されたURLからファームウェアをダウンロードして更新を行います。
                「ファームウェア更新」ボタンをクリックすると、即座に指定されたURLからファームウェアをダウンロードして更新を行います。
              </p>
            </div>
          </section>

          {/* 設定ファイル */}
          <section className="bg-white border rounded shadow p-6 space-y-6">
            <h2 className="text-lg font-bold text-gray-700">設定ファイル</h2>

            <div className="space-y-3">
              <h3 className="text-sm font-medium text-gray-600">設定ファイルをダウンロード</h3>
              <button
                onClick={handleDownload}
                className="px-4 py-2 bg-blue-500 text-white rounded hover:bg-blue-600 transition"
              >
                設定ファイルをダウンロード
              </button>
            </div>

            <div className="border-t pt-6 space-y-3">
              <h3 className="text-sm font-medium text-gray-600">設定ファイルをアップロード</h3>
              <div className="space-y-3">
                <label className="block">
                  <span className="text-sm text-gray-600 mb-1 block">JSONファイルを選択:</span>
                  <input
                    type="file"
                    accept=".json,application/json"
                    onChange={handleFileChange}
                    className="block w-full text-sm text-gray-500
                      file:mr-4 file:py-2 file:px-4
                      file:rounded file:border-0
                      file:text-sm file:font-medium
                      file:bg-gray-100 file:text-gray-700
                      hover:file:bg-gray-200 cursor-pointer"
                  />
                </label>
                {file && (
                  <p className="text-sm text-gray-500">選択中: {file.name}</p>
                )}
                <button
                  onClick={handleUpload}
                  disabled={!file}
                  className={`px-4 py-2 rounded transition ${file
                      ? "bg-blue-600 text-white hover:bg-blue-700"
                      : "bg-gray-300 text-gray-500 cursor-not-allowed"
                    }`}
                >
                  アップロード
                </button>
              </div>
            </div>
          </section>
        </div>
      )}

      {/* Syslog 設定タブ */}
      {activeTab === 'syslog' && (
        <section className="bg-white border rounded shadow p-6 space-y-4">
          <h2 className="text-lg font-bold text-gray-700">Syslog 設定</h2>
          <form onSubmit={handleSyslogSubmit} className="space-y-4">
            <div className="flex items-center space-x-3">
              <label className="block text-sm font-medium text-gray-700 mb-2 text-left">Syslog 有効</label>
              <button
                type="button"
                onClick={() => setSyslogEnabled(!syslogEnabled)}
                className={`relative inline-flex h-6 w-11 flex-shrink-0 cursor-pointer rounded-full border-2 border-transparent transition-colors duration-200 ease-in-out focus:outline-none focus:ring-2 focus:ring-blue-500 focus:ring-offset-2 ${syslogEnabled ? 'bg-blue-600' : 'bg-gray-200'}`}
                role="switch"
                aria-checked={syslogEnabled}
              >
                <span
                  className={`inline-block h-5 w-5 transform rounded-full bg-white shadow ring-0 transition duration-200 ease-in-out ${syslogEnabled ? 'translate-x-5' : 'translate-x-0'}`}
                />
              </button>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2 text-left">Syslog サーバー</label>
              <input
                type="text"
                value={syslogServer}
                onChange={(e) => setSyslogServer(e.target.value)}
                className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                placeholder="192.168.1.100"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2 text-left">ポート</label>
              <input
                type="text"
                value={syslogPort}
                onChange={(e) => setSyslogPort(e.target.value)}
                className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                placeholder="514"
              />
              </div>
  
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2 text-left">App Name</label>
                <input
                  type="text"
                  value={syslogAppName}
                  onChange={(e) => setSyslogAppName(e.target.value)}
                  className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                  placeholder="OTA"
                />
              </div>
  
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2 text-left">Facility</label>
              <select
                value={syslogFacility}
                onChange={(e) => setSyslogFacility(e.target.value)}
                className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
              >
                <option value="user">User</option>
                <option value="local0">Local0</option>
                <option value="local1">Local1</option>
                <option value="local2">Local2</option>
                <option value="local3">Local3</option>
                <option value="local4">Local4</option>
                <option value="local5">Local5</option>
                <option value="local6">Local6</option>
                <option value="local7">Local7</option>
              </select>
            </div>

            <button
              type="submit"
              className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 transition"
            >
              設定を保存
            </button>
          </form>
        </section>
      )}

      {/* NTP 設定タブ */}
      {activeTab === 'ntp' && (
        <section className="bg-white border rounded shadow p-6 space-y-4">
          <h2 className="text-lg font-bold text-gray-700">NTP 設定</h2>
          <form onSubmit={handleNtpSubmit} className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2 text-left">NTP サーバー</label>
              <input
                type="text"
                value={ntpServer}
                onChange={(e) => setNtpServer(e.target.value)}
                className="mt-1 block w-full border border-gray-300 rounded-md shadow-sm p-2"
                placeholder="ntp.nict.jp"
              />
            </div>

            <button
              type="submit"
              className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 transition"
            >
              設定を保存
            </button>
          </form>
        </section>
      )}

      {/* 結果表示セクション */}
      {result && (
        <section className="bg-white border rounded shadow p-6">
          <h2 className="text-lg font-bold text-gray-700 mb-2">結果</h2>
          <pre className="bg-gray-100 p-4 rounded text-sm overflow-x-auto">{result}</pre>
          {loading && (
            <div className="mt-4 flex items-center justify-center text-gray-600">
              <svg className="animate-spin h-5 w-5 mr-2" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
              </svg>
              <span>システム再起動中です...</span>
            </div>
          )}
        </section>
      )}
      {loading && (
        <div className="fixed inset-0 bg-black bg-opacity-30 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg p-6 shadow-xl">
            <div className="flex items-center justify-center mb-4">
              <svg className="animate-spin h-8 w-8 mr-3 text-blue-600" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
              </svg>
              <span className="text-lg font-semibold text-gray-700">システム再起動中</span>
            </div>
            <p className="text-gray-600 text-center">再接続まであと約10秒...</p>
          </div>
        </div>
      )}
      {otaLoading && (
        <div className="fixed inset-0 bg-black bg-opacity-30 flex items-center justify-center z-[100]">
          <div className="bg-white rounded-lg p-6 shadow-xl">
            <div className="flex items-center justify-center mb-4">
              <svg className="animate-spin h-8 w-8 mr-3 text-green-600" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
              </svg>
              <span className="text-lg font-semibold text-gray-700">ファームウェア更新中</span>
            </div>
            <p className="text-gray-600 text-center">再接続まであと約30秒...</p>
          </div>
        </div>
      )}
    </div>
  );
};

export default Settings;
