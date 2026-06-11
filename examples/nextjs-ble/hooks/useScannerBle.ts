import { useState, useCallback } from 'react';
import { scannerBle, ScannerMessage } from '../lib/scannerBle';

export type { ScannerMessage };

export function useScannerBle() {
  const [connected, setConnected] = useState(false);
  const [connecting, setConnecting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const connect = useCallback(async () => {
    setConnecting(true);
    setError(null);
    try {
      await scannerBle.connect();
      setConnected(true);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Connexion échouée');
    } finally {
      setConnecting(false);
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

  return { connected, connecting, error, connect, disconnect, send, subscribe };
}
