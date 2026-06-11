import { useState, useCallback } from 'react';
import { ScannerMessage } from '../hooks/useScannerBle';

export function WifiAddForm({ send, subscribe, ssid: initialSsid = '', onSaved, onCancel }: {
    send: (cmd: object) => Promise<void>;
    subscribe: (handler: (msg: ScannerMessage) => void) => () => void;
    ssid?: string;
    onSaved: () => void;
    onCancel: () => void;
}) {
    const [ssid, setSsid] = useState(initialSsid);
    const [password, setPassword] = useState('');
    const [priority, setPriority] = useState(5);
    const [showPassword, setShowPassword] = useState(false);
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);

    const save = useCallback(async () => {
        if (!ssid.trim()) return;
        setSaving(true);
        setError(null);

        const unsub = subscribe((msg) => {
            if (msg.t === 'ok') {
                unsub();
                setSaving(false);
                onSaved();
            } else if (msg.t === 'err') {
                unsub();
                setSaving(false);
                setError(msg.m ?? 'Erreur inconnue');
            }
        });

        try {
            await send({ t: 'save', ssid: ssid.trim(), pass: password, pri: priority });
        } catch (e) {
            unsub();
            setSaving(false);
            setError(e instanceof Error ? e.message : 'Erreur envoi');
        }
    }, [send, subscribe, ssid, password, priority, onSaved]);

    return (
        <div className="flex flex-col gap-4 p-4 border rounded-lg">
            <h2 className="font-semibold text-gray-800">Ajouter un réseau WiFi</h2>

            <div className="flex flex-col gap-1">
                <label className="text-sm text-gray-600">Nom du réseau (SSID)</label>
                <input
                    value={ssid}
                    onChange={(e) => setSsid(e.target.value)}
                    placeholder="MonReseau"
                    className="border rounded px-3 py-2 text-sm"
                />
            </div>

            <div className="flex flex-col gap-1">
                <label className="text-sm text-gray-600">Mot de passe</label>
                <div className="flex gap-2">
                    <input
                        type={showPassword ? 'text' : 'password'}
                        value={password}
                        onChange={(e) => setPassword(e.target.value)}
                        placeholder="Mot de passe"
                        className="flex-1 border rounded px-3 py-2 text-sm"
                    />
                    <button
                        type="button"
                        onClick={() => setShowPassword((v) => !v)}
                        className="px-3 text-sm text-gray-500 border rounded"
                    >
                        {showPassword ? 'Cacher' : 'Voir'}
                    </button>
                </div>
            </div>

            <div className="flex flex-col gap-1">
                <label className="text-sm text-gray-600">Priorité (1 = la plus haute)</label>
                <select
                    value={priority}
                    onChange={(e) => setPriority(Number(e.target.value))}
                    className="border rounded px-3 py-2 text-sm"
                >
                    {[1, 2, 3, 4, 5].map((p) => (
                        <option key={p} value={p}>{p}</option>
                    ))}
                </select>
            </div>

            {error && <p className="text-sm text-red-500">{error}</p>}

            <div className="flex gap-2 pt-1">
                <button
                    type="button"
                    onClick={onCancel}
                    className="flex-1 py-2 border rounded text-sm text-gray-700"
                >
                    Annuler
                </button>
                <button
                    type="button"
                    onClick={save}
                    disabled={saving || !ssid.trim()}
                    className="flex-1 py-2 rounded bg-blue-600 text-white text-sm disabled:opacity-50"
                >
                    {saving ? 'Sauvegarde...' : 'Sauvegarder'}
                </button>
            </div>
        </div>
    );
}
