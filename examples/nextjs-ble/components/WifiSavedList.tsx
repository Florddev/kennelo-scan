import { useState, useRef, useCallback } from 'react';
import { ScannerMessage } from '../hooks/useScannerBle';

interface SavedNetwork {
    ssid: string;
    priority: number;
}

export function WifiSavedList({ send, subscribe }: {
    send: (cmd: object) => Promise<void>;
    subscribe: (handler: (msg: ScannerMessage) => void) => () => void;
}) {
    const [loading, setLoading] = useState(false);
    const [networks, setNetworks] = useState<SavedNetwork[]>([]);
    const [deleting, setDeleting] = useState<string | null>(null);
    const foundRef = useRef<SavedNetwork[]>([]);

    const load = useCallback(async () => {
        setLoading(true);
        foundRef.current = [];
        setNetworks([]);

        const unsub = subscribe((msg) => {
            if (msg.t === 'saved' && msg.ssid) {
                foundRef.current.push({ ssid: msg.ssid, priority: msg.pri ?? 5 });
                setNetworks([...foundRef.current]);
            } else if (msg.t === 'list_done' || msg.t === 'err') {
                setLoading(false);
                unsub();
            }
        });

        try {
            await send({ t: 'list' });
        } catch {
            setLoading(false);
            unsub();
        }
    }, [send, subscribe]);

    const deleteNetwork = useCallback(
        async (ssid: string) => {
            setDeleting(ssid);

            const unsub = subscribe((msg) => {
                if (msg.t === 'ok' || msg.t === 'err') {
                    unsub();
                    setDeleting(null);
                    if (msg.t === 'ok') setNetworks((prev) => prev.filter((n) => n.ssid !== ssid));
                }
            });

            try {
                await send({ t: 'del', ssid });
            } catch {
                setDeleting(null);
                unsub();
            }
        },
        [send, subscribe]
    );

    return (
        <div className="flex flex-col gap-3">
            <div className="flex items-center justify-between">
                <h2 className="font-semibold text-gray-800">Réseaux sauvegardés</h2>
                <button
                    onClick={load}
                    disabled={loading}
                    className="px-3 py-1 text-sm rounded bg-blue-600 text-white disabled:opacity-50"
                >
                    {loading ? 'Chargement...' : 'Rafraîchir'}
                </button>
            </div>

            {networks.length === 0 && !loading && (
                <p className="text-sm text-gray-400 text-center py-6">Aucun réseau sauvegardé</p>
            )}

            <ul className="flex flex-col gap-2">
                {networks.map((net) => (
                    <li key={net.ssid} className="flex items-center justify-between p-3 border rounded">
                        <div>
                            <span className="font-medium text-sm">{net.ssid}</span>
                            <span className="ml-2 text-xs text-gray-400">priorité {net.priority}</span>
                        </div>
                        <button
                            onClick={() => deleteNetwork(net.ssid)}
                            disabled={deleting === net.ssid}
                            className="text-sm text-red-500 disabled:opacity-40"
                        >
                            {deleting === net.ssid ? '...' : 'Supprimer'}
                        </button>
                    </li>
                ))}
            </ul>
        </div>
    );
}
