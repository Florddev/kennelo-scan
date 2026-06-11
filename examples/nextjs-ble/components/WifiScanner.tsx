import { useState, useRef, useCallback } from 'react';
import { ScannerMessage } from '../hooks/useScannerBle';

interface WifiNetwork {
    ssid: string;
    rssi: number;
}

function signalBars(rssi: number): string {
    if (rssi > -50) return '▂▄▆█';
    if (rssi > -60) return '▂▄▆░';
    if (rssi > -70) return '▂▄░░';
    return '▂░░░';
}

export function WifiScanner({ send, subscribe, onSelect }: {
    send: (cmd: object) => Promise<void>;
    subscribe: (handler: (msg: ScannerMessage) => void) => () => void;
    onSelect: (ssid: string) => void;
}) {
    const [scanning, setScanning] = useState(false);
    const [networks, setNetworks] = useState<WifiNetwork[]>([]);
    const foundRef = useRef<WifiNetwork[]>([]);

    const scan = useCallback(async () => {
        setScanning(true);
        foundRef.current = [];
        setNetworks([]);

        const unsub = subscribe((msg) => {
            if (msg.t === 'net' && msg.ssid) {
                foundRef.current.push({ ssid: msg.ssid, rssi: msg.rssi ?? 0 });
                setNetworks([...foundRef.current]);
            } else if (msg.t === 'scan_done' || msg.t === 'err') {
                setScanning(false);
                unsub();
            }
        });

        try {
            await send({ t: 'scan' });
        } catch {
            setScanning(false);
            unsub();
        }
    }, [send, subscribe]);

    return (
        <div className="flex flex-col gap-3">
            <div className="flex items-center justify-between">
                <h2 className="font-semibold text-gray-800">Réseaux disponibles</h2>
                <button
                    onClick={scan}
                    disabled={scanning}
                    className="px-3 py-1 text-sm rounded bg-blue-600 text-white disabled:opacity-50"
                >
                    {scanning ? 'Scan...' : 'Scanner'}
                </button>
            </div>

            {networks.length === 0 && !scanning && (
                <p className="text-sm text-gray-400 text-center py-6">
                    Lancez un scan pour voir les réseaux disponibles
                </p>
            )}

            <ul className="flex flex-col gap-2">
                {networks.map((net) => (
                    <li
                        key={net.ssid}
                        onClick={() => onSelect(net.ssid)}
                        className="flex items-center justify-between p-3 border rounded cursor-pointer hover:bg-gray-50 active:bg-gray-100"
                    >
                        <span className="font-medium text-sm">{net.ssid}</span>
                        <span className="text-xs text-gray-400 font-mono">{signalBars(net.rssi)}</span>
                    </li>
                ))}
            </ul>
        </div>
    );
}
