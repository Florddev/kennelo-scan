'use client';

import { useState } from 'react';
import { useScannerBle } from './hooks/useScannerBle';
import { ScannerConnect } from './components/ScannerConnect';
import { WifiScanner } from './components/WifiScanner';
import { WifiSavedList } from './components/WifiSavedList';
import { WifiAddForm } from './components/WifiAddForm';

export default function ScannerConfigPage() {
  const { connected, connecting, scanning, devices, error, startScan, connectTo, disconnect, send, subscribe } = useScannerBle();
  const [selectedSsid, setSelectedSsid] = useState<string | null>(null);

  return (
    <main className="max-w-md mx-auto p-4 flex flex-col gap-6">
      <h1 className="text-xl font-bold text-gray-900">Configuration KenoTag Scanner</h1>

      <ScannerConnect
        connected={connected}
        connecting={connecting}
        scanning={scanning}
        devices={devices}
        error={error}
        onScan={startScan}
        onConnect={connectTo}
        onDisconnect={disconnect}
      />

      {connected && (
        <>
          {selectedSsid !== null ? (
            <WifiAddForm
              send={send}
              subscribe={subscribe}
              ssid={selectedSsid}
              onSaved={() => setSelectedSsid(null)}
              onCancel={() => setSelectedSsid(null)}
            />
          ) : (
            <>
              <WifiScanner send={send} subscribe={subscribe} onSelect={setSelectedSsid} />
              <WifiSavedList send={send} subscribe={subscribe} />
            </>
          )}
        </>
      )}
    </main>
  );
}
