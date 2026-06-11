export function ScannerConnect({ connected, connecting, error, onConnect, onDisconnect }: {
    connected: boolean;
    connecting: boolean;
    error: string | null;
    onConnect: () => void;
    onDisconnect: () => void;
}) {
    return (
        <div className="flex flex-col items-center gap-3 p-4 border rounded-lg">
            <div className={`w-3 h-3 rounded-full ${connected ? 'bg-green-500' : 'bg-gray-300'}`} />
            <span className="text-sm text-gray-600">
                {connected ? 'Scanner connecté' : connecting ? 'Connexion en cours...' : 'Scanner déconnecté'}
            </span>
            {error && <p className="text-sm text-red-500">{error}</p>}
            <button
                onClick={connected ? onDisconnect : onConnect}
                disabled={connecting}
                className="px-4 py-2 rounded bg-blue-600 text-white text-sm disabled:opacity-50"
            >
                {connected ? 'Déconnecter' : connecting ? 'Connexion...' : 'Connecter le scanner'}
            </button>
        </div>
    );
}
