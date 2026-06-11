// npm install @capacitor-community/bluetooth-le
import { BleClient } from '@capacitor-community/bluetooth-le';

const SERVICE_UUID = '19b10000-e8f2-537e-4f6c-d104768a1214';
const TX_UUID      = '19b10001-e8f2-537e-4f6c-d104768a1214';
const RX_UUID      = '19b10002-e8f2-537e-4f6c-d104768a1214';

export interface ScannerMessage {
  t: string;
  ssid?: string;
  rssi?: number;
  pri?: number;
  m?: string;
}

type MessageHandler = (msg: ScannerMessage) => void;

function toDataView(text: string): DataView {
  const bytes = new TextEncoder().encode(text);
  return new DataView(bytes.buffer);
}

function fromDataView(value: DataView): string {
  return new TextDecoder().decode(value);
}

class ScannerBle {
  private deviceId: string | null = null;
  private buffer = '';
  private handlers = new Set<MessageHandler>();

  async connect(): Promise<void> {
    await BleClient.initialize();
    const device = await BleClient.requestDevice({ services: [SERVICE_UUID] });
    this.deviceId = device.deviceId;
    await BleClient.connect(this.deviceId, () => {
      this.deviceId = null;
      this.buffer = '';
      this.handlers.clear();
    });
    await BleClient.startNotifications(this.deviceId, SERVICE_UUID, TX_UUID, (v) =>
      this.onData(v)
    );
  }

  async disconnect(): Promise<void> {
    if (!this.deviceId) return;
    try {
      await BleClient.stopNotifications(this.deviceId, SERVICE_UUID, TX_UUID);
      await BleClient.disconnect(this.deviceId);
    } finally {
      this.deviceId = null;
      this.buffer = '';
    }
  }

  get isConnected(): boolean {
    return this.deviceId !== null;
  }

  subscribe(handler: MessageHandler): () => void {
    this.handlers.add(handler);
    return () => this.handlers.delete(handler);
  }

  async send(cmd: object): Promise<void> {
    if (!this.deviceId) throw new Error('Not connected');
    await BleClient.write(this.deviceId, SERVICE_UUID, RX_UUID, toDataView(JSON.stringify(cmd)));
  }

  private onData(value: DataView): void {
    this.buffer += fromDataView(value);
    const lines = this.buffer.split('\n');
    this.buffer = lines.pop() ?? '';
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed) continue;
      try {
        const msg = JSON.parse(trimmed) as ScannerMessage;
        this.handlers.forEach((h) => h(msg));
      } catch {
        // ignore malformed chunks
      }
    }
  }
}

export const scannerBle = new ScannerBle();
