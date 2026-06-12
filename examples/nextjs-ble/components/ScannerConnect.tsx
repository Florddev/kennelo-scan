import { FoundDevice } from '../hooks/useScannerBle';

export function ScannerConnect({ connected, connecting, scanning, devices, error, onScan, onConnect, onDisconnect }: {
    connected: boolean;
    connecting: boolean;
    scanning: boolean;
    devices: FoundDevice[];
    error: string | null;
    onScan: () => void;
    onConnect: (deviceId: string) => void;
    onDisconnect: () => void;
}) {
    return (
        <div className="flex flex-col gap-3 p-4 border rounded-lg">
            <div className="flex items-center gap-2">
                <div className={`w-3 h-3 rounded-full ${connected ? 'bg-green-500' : 'bg-gray-300'}`} />
                <span className="text-sm text-gray-600">
                    {connected ? 'Scanner connecté' : connecting ? 'Connexion en cours...' : 'Scanner déconnecté'}
                </span>
            </div>

            {error && (
                <div className="flex items-center gap-2 p-3 bg-red-50 border border-red-200 rounded-lg">
                    <span className="text-red-500">⚠️</span>
                    <p className="text-sm text-red-600">{error}</p>
                </div>
            )}

            {!connected && (
                <>
                    <button
                        onClick={onScan}
                        disabled={scanning || connecting}
                        className="px-4 py-2 rounded bg-blue-600 text-white text-sm disabled:opacity-50"
                    >
                        {scanning ? 'Recherche en cours...' : 'Rechercher le scanner'}
                    </button>

                    {devices.length > 0 && (
                        <ul className="flex flex-col gap-2">
                            {devices.map((d) => (
                                <li key={d.deviceId}>
                                    <button
                                        onClick={() => onConnect(d.deviceId)}
                                        disabled={connecting}
                                        className="w-full flex items-center justify-between p-3 border rounded-lg hover:bg-gray-50 disabled:opacity-50"
                                    >
                                        <span className="text-sm font-medium">{d.name}</span>
                                        <span className="text-xs text-gray-400">{d.rssi} dBm</span>
                                    </button>
                                </li>
                            ))}
                        </ul>
                    )}

                    {scanning && devices.length === 0 && (
                        <p className="text-sm text-gray-400 text-center py-2">
                            Assurez-vous que le scanner est en mode Bluetooth...
                        </p>
                    )}
                </>
            )}

            {connected && (
                <button
                    onClick={onDisconnect}
                    className="px-4 py-2 rounded border border-gray-300 text-sm text-gray-600"
                >
                    Déconnecter
                </button>
            )}
        </div>
    );
}
