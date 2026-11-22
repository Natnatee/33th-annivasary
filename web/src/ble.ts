// BLE UUIDs for 33rd Anniversary ESP32-S3
export const SERVICE_UUID = '19b10000-e8f2-537e-4f6c-d104768a1214';
export const CHAR_SPEED_UUID = '19b10001-e8f2-537e-4f6c-d104768a1214';
export const CHAR_THRESHOLD_UUID = '19b10002-e8f2-537e-4f6c-d104768a1214';
export const CHAR_VOLUME_UUID = '19b10003-e8f2-537e-4f6c-d104768a1214';
export const CHAR_STATE_UUID = '19b10004-e8f2-537e-4f6c-d104768a1214';
export const CHAR_PLAYMODE_UUID = '19b10005-e8f2-537e-4f6c-d104768a1214';
export const CHAR_LIGHTMODE_UUID = '19b10006-e8f2-537e-4f6c-d104768a1214';

export interface DeviceConfig {
  speed: number;       // Step delay ms (0 = stop, 1 - 10)
  threshold: number;   // Percentage (50 - 99) -> 0.50 - 0.99
  volume: number;      // 0 - 30
  isRunning: boolean;  // 0 or 1
  playMode: number;    // 0 = Sequential, 1 = Random/Shuffle
  lightMode: number;   // 0 = Breathing (Fade), 1 = Solid ON, 2 = Blink
}

export class BLEService {
  private device: BluetoothDevice | null = null;
  private server: BluetoothRemoteGATTServer | null = null;
  private charSpeed: BluetoothRemoteGATTCharacteristic | null = null;
  private charThreshold: BluetoothRemoteGATTCharacteristic | null = null;
  private charVolume: BluetoothRemoteGATTCharacteristic | null = null;
  private charState: BluetoothRemoteGATTCharacteristic | null = null;
  private charPlayMode: BluetoothRemoteGATTCharacteristic | null = null;
  private charLightMode: BluetoothRemoteGATTCharacteristic | null = null;

  public onDisconnectCallback: (() => void) | null = null;
  public onStateChangeCallback: ((isRunning: boolean) => void) | null = null;

  async connect(): Promise<DeviceConfig> {
    if (!navigator.bluetooth) {
      throw new Error('เบราว์เซอร์นี้ไม่รองรับ Web Bluetooth (กรุณาใช้ Google Chrome หรือ Edge)');
    }

    // Request Bluetooth Device
    this.device = await navigator.bluetooth.requestDevice({
      filters: [
        { namePrefix: '33rd' },
        { namePrefix: 'Anniversary' },
        { services: [SERVICE_UUID] },
      ],
      optionalServices: [SERVICE_UUID],
    });

    this.device.addEventListener('gattserverdisconnected', () => {
      this.cleanup();
      if (this.onDisconnectCallback) this.onDisconnectCallback();
    });

    if (!this.device.gatt) {
      throw new Error('ไม่พบ GATT Server บนอุปกรณ์');
    }

    this.server = await this.device.gatt.connect();
    const service = await this.server.getPrimaryService(SERVICE_UUID);

    // Get Characteristics
    this.charSpeed = await service.getCharacteristic(CHAR_SPEED_UUID);
    this.charThreshold = await service.getCharacteristic(CHAR_THRESHOLD_UUID);
    this.charVolume = await service.getCharacteristic(CHAR_VOLUME_UUID);
    this.charState = await service.getCharacteristic(CHAR_STATE_UUID);

    let playModeVal = 0;
    try {
      this.charPlayMode = await service.getCharacteristic(CHAR_PLAYMODE_UUID);
      playModeVal = (await this.charPlayMode.readValue()).getUint8(0);
    } catch {
      this.charPlayMode = null;
    }

    let lightModeVal = 0;
    try {
      this.charLightMode = await service.getCharacteristic(CHAR_LIGHTMODE_UUID);
      lightModeVal = (await this.charLightMode.readValue()).getUint8(0);
    } catch {
      this.charLightMode = null;
    }

    // Read initial values
    const speedVal = (await this.charSpeed.readValue()).getUint8(0);
    const threshVal = (await this.charThreshold.readValue()).getUint8(0);
    const volVal = (await this.charVolume.readValue()).getUint8(0);
    const stateVal = (await this.charState.readValue()).getUint8(0);

    // Subscribe to state notifications
    await this.charState.startNotifications();
    this.charState.addEventListener('characteristicvaluechanged', (event: Event) => {
      const target = event.target as unknown as BluetoothRemoteGATTCharacteristic;
      if (target.value) {
        const isRunning = target.value.getUint8(0) === 1;
        if (this.onStateChangeCallback) this.onStateChangeCallback(isRunning);
      }
    });

    return {
      speed: typeof speedVal === 'number' ? speedVal : 3,
      threshold: threshVal || 80,
      volume: volVal !== undefined ? volVal : 20,
      isRunning: stateVal === 1,
      playMode: playModeVal || 0,
      lightMode: lightModeVal || 0,
    };
  }

  async writeSpeed(speed: number): Promise<void> {
    if (!this.charSpeed) return;
    const data = new Uint8Array([Math.min(Math.max(speed, 0), 10)]);
    await this.charSpeed.writeValue(data);
  }

  async writeThreshold(threshPercent: number): Promise<void> {
    if (!this.charThreshold) return;
    const data = new Uint8Array([Math.min(Math.max(threshPercent, 50), 99)]);
    await this.charThreshold.writeValue(data);
  }

  async writeVolume(vol: number): Promise<void> {
    if (!this.charVolume) return;
    const data = new Uint8Array([Math.min(Math.max(vol, 0), 30)]);
    await this.charVolume.writeValue(data);
  }

  async writeState(isRunning: boolean): Promise<void> {
    if (!this.charState) return;
    const data = new Uint8Array([isRunning ? 1 : 0]);
    await this.charState.writeValue(data);
  }

  async writePlayMode(mode: number): Promise<void> {
    if (!this.charPlayMode) return;
    const data = new Uint8Array([mode === 1 ? 1 : 0]);
    await this.charPlayMode.writeValue(data);
  }

  async writeLightMode(mode: number): Promise<void> {
    if (!this.charLightMode) return;
    const data = new Uint8Array([Math.min(Math.max(mode, 0), 2)]);
    await this.charLightMode.writeValue(data);
  }

  disconnect(): void {
    if (this.device && this.device.gatt?.connected) {
      this.device.gatt.disconnect();
    }
    this.cleanup();
  }

  private cleanup(): void {
    this.device = null;
    this.server = null;
    this.charSpeed = null;
    this.charThreshold = null;
    this.charVolume = null;
    this.charState = null;
    this.charPlayMode = null;
    this.charLightMode = null;
  }
}

export const bleService = new BLEService();
