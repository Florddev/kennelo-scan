import { useState, useCallback } from 'react';
import { scannerBle, ScannerMessage, FoundDevice } from '../lib/scannerBle';

export type { ScannerMessage, FoundDevice };

export function useScannerBle() {
  const [connected, setConnected] = useState(false);
  const [connecting, setConnecting] = useState(false);
  const [scanning, setScanning] = useState(false);
  const [devices, setDevices] = useState<FoundDevice[]>([]);
  const [error, setError] = useState<string | null>(null);

  const startScan = useCallback(async () => {
    setScanning(true);
    setDevices([]);
    setError(null);
    try {
      await scannerBle.scanDevices((device) => {
        setDevices((prev) => {
          if (prev.find((d) => d.deviceId === device.deviceId)) return prev;
          return [...prev, device];
        });
      });
      setTimeout(() => setScanning(false), 8000);
    } catch (e) {
      const msg = e instanceof Error ? e.message : '';
      setError(msg === 'BLUETOOTH_DISABLED'
        ? 'Veuillez activer le Bluetooth pour connecter le scanner.'
        : 'Erreur lors du scan BLE.');
      setScanning(false);
    }
  }, []);

  const connectTo = useCallback(async (deviceId: string) => {
    setConnecting(true);
    setError(null);
    try {
      await scannerBle.stopScan();
      await scannerBle.connectTo(deviceId);
      setConnected(true);
      setDevices([]);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Connexion échouée');
    } finally {
      setConnecting(false);
      setScanning(false);
    }
  }, []);

  const disconnect = useCallback(async () => {
    await scannerBle.disconnect();
    setConnected(false);
  }, []);

  const send = useCallback((cmd: object) => scannerBle.send(cmd), []);
  const subscribe = useCallback(
    (handler: (msg: ScannerMessage) => void) => scannerBle.subscribe(handler),
    []
  );

  return { connected, connecting, scanning, devices, error, startScan, connectTo, disconnect, send, subscribe };
}
