import React, { useState, useEffect } from 'react';

interface DeviceStatus {
  hostname: string;
  ip_address: string;
  subnet_mask: string;
  gateway: string;
  dns_server: string;
  system_time: string;
  status: string;
  firmware_version?: string;
  build_date?: string;
}

const HomePage: React.FC = () => {
  const [deviceStatus, setDeviceStatus] = useState<DeviceStatus | null>(null);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchDeviceStatus = async () => {
      try {
        setLoading(true);
        const response = await fetch('/api/status');
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data: DeviceStatus = await response.json();
        setDeviceStatus(data);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Unknown error occurred');
        console.error('Error fetching device status:', err);
      } finally {
        setLoading(false);
      }
    };

    // Fetch immediately on component mount
    fetchDeviceStatus();

    // Then fetch every 5 seconds
    const interval = setInterval(fetchDeviceStatus, 5000);

    return () => clearInterval(interval);
  }, []);

  // Loading state
  if (loading && !deviceStatus) {
    return (
      <div className="max-w-5xl mx-auto px-4 py-6">
        <header className="text-2xl font-semibold text-gray-800 border-b pb-2 mb-6">
          デバイスステータス
        </header>
        <div className="flex justify-center items-center h-32">
          <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-gray-900"></div>
          <span className="ml-2">データを読み込んでいます...</span>
        </div>
      </div>
    );
  }

  // Error state
  if (error) {
    return (
      <div className="max-w-5xl mx-auto px-4 py-6">
        <header className="text-2xl font-semibold text-gray-800 border-b pb-2 mb-6">
          デバイスステータス
        </header>
        <div className="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded relative" role="alert">
          <strong className="font-bold">エラー: </strong>
          <span className="block sm:inline">{error}</span>
        </div>
      </div>
    );
  }

  return (
    <div className="max-w-5xl mx-auto px-4 py-6">
      <header className="text-2xl font-semibold text-gray-800 border-b pb-2 mb-6">
        デバイスステータス
      </header>

      <div className="bg-white border rounded shadow overflow-hidden mb-6">
        <table className="min-w-full divide-y divide-gray-200">
          <thead className="bg-gray-50">
            <tr>
              <th scope="col" className="px-6 py-3 text-center text-xs font-medium text-gray-500 uppercase tracking-wider">
                項目
              </th>
              <th scope="col" className="px-6 py-3 text-center text-xs font-medium text-gray-500 uppercase tracking-wider">
                値
              </th>
            </tr>
          </thead>
          <tbody className="bg-white divide-y divide-gray-200">
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                デバイス名
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.hostname || 'ESP32-OTA'}
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                ファームウェアバージョン
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.firmware_version || '-'}
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                ビルド日時
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.build_date || '-'}
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                ステータス
              </td>
              <td className="px-6 py-4 whitespace-nowrap">
                <span className="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium text-white bg-green-500">
                  オンライン
                </span>
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                IPアドレス
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.ip_address || '-'}
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                サブネットマスク
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.subnet_mask || '-'}
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                ゲートウェイ
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.gateway || '-'}
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                DNSサーバ
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.dns_server || '-'}
              </td>
            </tr>
            <tr className="hover:bg-gray-50">
              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                システム時刻
              </td>
              <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                {deviceStatus?.system_time || '-'}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default HomePage;
