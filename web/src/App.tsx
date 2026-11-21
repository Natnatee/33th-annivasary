import { useState, useEffect } from 'react';
import { 
  Bluetooth, 
  BluetoothConnected, 
  Power, 
  Volume2, 
  VolumeX, 
  Mic, 
  RotateCw,
  Pause,
  Check,
  Repeat,
  Shuffle,
  Music
} from 'lucide-react';
import { bleService } from './ble';
import type { DeviceConfig } from './ble';

// Mapping tables for levels 1 to 5
const THRESHOLD_LEVELS = [65, 72, 80, 86, 92]; // Index 0..4 -> Levels 1..5
const VOLUME_LEVELS = [0, 6, 12, 18, 24, 30];   // 0 = Mute, 1..5

function getSpeedLevel(delay: number): number {
  if (delay === 0) return 0;
  const lvl = 6 - delay;
  return Math.max(1, Math.min(5, lvl));
}

function getThresholdLevel(thresh: number): number {
  let closestIdx = 0;
  let minDiff = 999;
  for (let i = 0; i < THRESHOLD_LEVELS.length; i++) {
    const diff = Math.abs(THRESHOLD_LEVELS[i] - thresh);
    if (diff < minDiff) {
      minDiff = diff;
      closestIdx = i;
    }
  }
  return closestIdx + 1; // Level 1..5
}

function getVolumeLevel(vol: number): number {
  if (vol === 0) return 0;
  let closestIdx = 1;
  let minDiff = 999;
  for (let i = 1; i < VOLUME_LEVELS.length; i++) {
    const diff = Math.abs(VOLUME_LEVELS[i] - vol);
    if (diff < minDiff) {
      minDiff = diff;
      closestIdx = i;
    }
  }
  return closestIdx;
}

export default function App() {
  const [isConnected, setIsConnected] = useState(false);
  const [connecting, setConnecting] = useState(false);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);

  // Simple levels for elderly users
  const [speedLevel, setSpeedLevel] = useState<number>(3);       // Level 0..5 (Default 3)
  const [micLevel, setMicLevel] = useState<number>(3);           // Level 1..5 (Default 3)
  const [volLevel, setVolLevel] = useState<number>(3);           // Level 0..5 (Default 3)
  const [playMode, setPlayMode] = useState<number>(0);           // 0 = Sequential, 1 = Random
  const [isRunning, setIsRunning] = useState<boolean>(false);

  useEffect(() => {
    bleService.onDisconnectCallback = () => {
      setIsConnected(false);
    };

    bleService.onStateChangeCallback = (state) => {
      setIsRunning(state);
    };
  }, []);

  const handleConnect = async () => {
    try {
      setConnecting(true);
      setErrorMsg(null);
      const config: DeviceConfig = await bleService.connect();
      setSpeedLevel(getSpeedLevel(config.speed));
      setMicLevel(getThresholdLevel(config.threshold));
      setVolLevel(getVolumeLevel(config.volume));
      setPlayMode(config.playMode || 0);
      setIsRunning(config.isRunning);
      setIsConnected(true);
    } catch (err: unknown) {
      const e = err as Error;
      if (e.name !== 'NotFoundError') {
        setErrorMsg('ไม่พบอุปกรณ์ กรุณาเสียบปลั๊กกล่องใหม่แล้วกดเชื่อมต่อ');
      }
    } finally {
      setConnecting(false);
    }
  };

  const handleDisconnect = () => {
    bleService.disconnect();
    setIsConnected(false);
  };

  const handleSelectSpeed = async (lvl: number) => {
    setSpeedLevel(lvl);
    const delay = lvl === 0 ? 0 : (6 - lvl); // Level 0 -> 0 (stop), Level 1 -> 5ms (slow), Level 5 -> 1ms (fast)
    try {
      await bleService.writeSpeed(delay);
    } catch (e) {
      console.error(e);
    }
  };

  const handleSelectMic = async (lvl: number) => {
    setMicLevel(lvl);
    const thresh = THRESHOLD_LEVELS[lvl - 1]; // Level 1 -> 65, Level 5 -> 92
    try {
      await bleService.writeThreshold(thresh);
    } catch (e) {
      console.error(e);
    }
  };

  const handleSelectVolume = async (lvl: number) => {
    setVolLevel(lvl);
    const vol = VOLUME_LEVELS[lvl]; // Level 0 -> 0, Level 5 -> 30
    try {
      await bleService.writeVolume(vol);
    } catch (e) {
      console.error(e);
    }
  };

  const handleSelectPlayMode = async (mode: number) => {
    setPlayMode(mode);
    try {
      await bleService.writePlayMode(mode);
    } catch (e) {
      console.error(e);
    }
  };

  const handleToggleState = async () => {
    const nextState = !isRunning;
    setIsRunning(nextState);
    try {
      await bleService.writeState(nextState);
    } catch (e) {
      console.error(e);
      setIsRunning(!nextState);
    }
  };

  return (
    <div className="min-h-screen bg-stone-100 text-stone-800 flex flex-col items-center justify-start p-4 sm:p-6 pb-12 select-none">
      {/* Top Header */}
      <header className="w-full max-w-md text-center pt-4 pb-4">
        <h1 className="text-2xl sm:text-3xl font-extrabold text-stone-800 tracking-tight">
          กล่องดนตรี 33 ปี ❤️
        </h1>
      </header>

      {/* Main Container */}
      <main className="w-full max-w-md space-y-5">
        
        {/* Connect Button Card */}
        <div className="bg-white rounded-3xl p-5 shadow-sm border border-stone-200">
          {!isConnected ? (
            <button
              onClick={handleConnect}
              disabled={connecting}
              className="w-full py-5 px-6 rounded-2xl bg-amber-600 active:bg-amber-700 text-white font-bold text-xl shadow-md flex items-center justify-center gap-3 cursor-pointer transition-transform active:scale-[0.98] disabled:opacity-60"
            >
              {connecting ? (
                <>
                  <span className="w-6 h-6 border-3 border-white/30 border-t-white rounded-full animate-spin" />
                  <span>กำลังค้นหา...</span>
                </>
              ) : (
                <>
                  <Bluetooth className="w-7 h-7" />
                  <span>กดเชื่อมต่อ</span>
                </>
              )}
            </button>
          ) : (
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className="w-12 h-12 rounded-2xl bg-emerald-100 text-emerald-700 flex items-center justify-center">
                  <BluetoothConnected className="w-7 h-7" />
                </div>
                <div>
                  <div className="text-lg font-bold text-emerald-700 flex items-center gap-1.5">
                    <Check className="w-5 h-5 stroke-[3]" />
                    <span>เชื่อมต่อแล้ว</span>
                  </div>
                  <p className="text-sm text-stone-500">พร้อมใช้งาน</p>
                </div>
              </div>

              <button
                onClick={handleDisconnect}
                className="px-4 py-2.5 rounded-xl bg-stone-100 active:bg-stone-200 text-stone-600 font-medium text-sm cursor-pointer"
              >
                เลิกเชื่อมต่อ
              </button>
            </div>
          )}

          {errorMsg && (
            <div className="mt-3 p-3.5 rounded-xl bg-rose-50 text-rose-700 text-sm font-medium text-center">
              {errorMsg}
            </div>
          )}
        </div>

        {/* Big On/Off Toggle Button */}
        <div className={`bg-white rounded-3xl p-6 shadow-sm border border-stone-200 transition-opacity duration-300 ${!isConnected ? 'opacity-40 pointer-events-none' : ''}`}>
          <div className="text-center mb-4">
            <span className="text-base font-bold text-stone-600">สวิตช์เปิด / ปิด</span>
          </div>

          <button
            onClick={handleToggleState}
            className={`w-full py-6 rounded-2xl font-bold text-2xl flex items-center justify-center gap-4 shadow-lg transition-all active:scale-[0.98] cursor-pointer ${
              isRunning
                ? 'bg-emerald-600 active:bg-emerald-700 text-white shadow-emerald-600/25 ring-4 ring-emerald-100'
                : 'bg-stone-200 active:bg-stone-300 text-stone-700'
            }`}
          >
            <Power className="w-8 h-8 stroke-[2.5]" />
            <span>{isRunning ? 'กำลังเปิดทำงาน' : 'กดเพื่อเปิด'}</span>
          </button>
        </div>

        {/* Setting 1: Speed (หมุนช้า - เร็ว + ไม่หมุน) */}
        <div className={`bg-white rounded-3xl p-5 shadow-sm border border-stone-200 space-y-3 transition-opacity duration-300 ${!isConnected ? 'opacity-40 pointer-events-none' : ''}`}>
          <div className="flex items-center gap-2.5">
            <RotateCw className="w-6 h-6 text-amber-600" />
            <h2 className="text-lg font-bold text-stone-800">ความเร็วหมุน</h2>
          </div>

          {/* Stop Button + Level 1 to 5 Buttons */}
          <div className="grid grid-cols-6 gap-1.5 pt-1">
            {/* Stop Rotation Button */}
            <button
              onClick={() => handleSelectSpeed(0)}
              className={`py-3.5 rounded-2xl font-bold flex items-center justify-center transition-all cursor-pointer ${
                speedLevel === 0
                  ? 'bg-amber-600 text-white shadow-md shadow-amber-600/30 scale-105'
                  : 'bg-stone-100 active:bg-stone-200 text-stone-500'
              }`}
              title="ไม่หมุน"
            >
              <Pause className="w-5 h-5 stroke-[2.5]" />
            </button>

            {[1, 2, 3, 4, 5].map((lvl) => (
              <button
                key={lvl}
                onClick={() => handleSelectSpeed(lvl)}
                className={`py-3.5 rounded-2xl font-extrabold text-xl transition-all cursor-pointer ${
                  speedLevel === lvl
                    ? 'bg-amber-600 text-white shadow-md shadow-amber-600/30 scale-105'
                    : 'bg-stone-100 active:bg-stone-200 text-stone-700'
                }`}
              >
                {lvl}
              </button>
            ))}
          </div>

          <div className="flex justify-between text-sm font-semibold text-stone-400 px-1 pt-0.5">
            <span>ไม่หมุน / 🐢 ช้า</span>
            <span>เร็วสุด 🐇</span>
          </div>
        </div>

        {/* Setting 2: Play Mode (เล่นเรียง 🔁 vs สุ่มเพลง 🔀) */}
        <div className={`bg-white rounded-3xl p-5 shadow-sm border border-stone-200 space-y-3 transition-opacity duration-300 ${!isConnected ? 'opacity-40 pointer-events-none' : ''}`}>
          <div className="flex items-center gap-2.5">
            <Music className="w-6 h-6 text-teal-600" />
            <h2 className="text-lg font-bold text-stone-800">โหมดเล่นเพลง</h2>
          </div>

          <div className="grid grid-cols-2 gap-3 pt-1">
            <button
              onClick={() => handleSelectPlayMode(0)}
              className={`py-4 px-3 rounded-2xl font-bold text-base flex flex-col items-center justify-center gap-2 transition-all cursor-pointer ${
                playMode === 0
                  ? 'bg-teal-600 text-white shadow-md shadow-teal-600/30 scale-102 ring-2 ring-teal-200'
                  : 'bg-stone-100 active:bg-stone-200 text-stone-700'
              }`}
            >
              <Repeat className="w-7 h-7 stroke-[2.5]" />
              <span>เล่นเรียง (1 ➡️ 7)</span>
            </button>

            <button
              onClick={() => handleSelectPlayMode(1)}
              className={`py-4 px-3 rounded-2xl font-bold text-base flex flex-col items-center justify-center gap-2 transition-all cursor-pointer ${
                playMode === 1
                  ? 'bg-teal-600 text-white shadow-md shadow-teal-600/30 scale-102 ring-2 ring-teal-200'
                  : 'bg-stone-100 active:bg-stone-200 text-stone-700'
              }`}
            >
              <Shuffle className="w-7 h-7 stroke-[2.5]" />
              <span>สุ่มเพลง</span>
            </button>
          </div>
        </div>

        {/* Setting 3: Mic Sensitivity (ติดง่าย - ติดยาก) */}
        <div className={`bg-white rounded-3xl p-5 shadow-sm border border-stone-200 space-y-3 transition-opacity duration-300 ${!isConnected ? 'opacity-40 pointer-events-none' : ''}`}>
          <div className="flex items-center gap-2.5">
            <Mic className="w-6 h-6 text-rose-600" />
            <h2 className="text-lg font-bold text-stone-800">เสียงเรียก "สามสิบสามปี"</h2>
          </div>

          {/* Level 1 to 5 Buttons */}
          <div className="grid grid-cols-5 gap-2 pt-1">
            {[1, 2, 3, 4, 5].map((lvl) => (
              <button
                key={lvl}
                onClick={() => handleSelectMic(lvl)}
                className={`py-3.5 rounded-2xl font-extrabold text-xl transition-all cursor-pointer ${
                  micLevel === lvl
                    ? 'bg-rose-600 text-white shadow-md shadow-rose-600/30 scale-105'
                    : 'bg-stone-100 active:bg-stone-200 text-stone-700'
                }`}
              >
                {lvl}
              </button>
            ))}
          </div>

          <div className="flex justify-between text-sm font-semibold text-stone-400 px-1 pt-0.5">
            <span>ติดง่าย</span>
            <span>ติดยาก</span>
          </div>
        </div>

        {/* Setting 4: Volume */}
        <div className={`bg-white rounded-3xl p-5 shadow-sm border border-stone-200 space-y-3 transition-opacity duration-300 ${!isConnected ? 'opacity-40 pointer-events-none' : ''}`}>
          <div className="flex items-center gap-2.5">
            <Volume2 className="w-6 h-6 text-indigo-600" />
            <h2 className="text-lg font-bold text-stone-800">ระดับเสียงเพลง</h2>
          </div>

          {/* Mute + Level 1 to 5 Buttons */}
          <div className="grid grid-cols-6 gap-1.5 pt-1">
            {/* Mute Button */}
            <button
              onClick={() => handleSelectVolume(0)}
              className={`py-3.5 rounded-2xl font-bold flex items-center justify-center transition-all cursor-pointer ${
                volLevel === 0
                  ? 'bg-indigo-600 text-white shadow-md shadow-indigo-600/30 scale-105'
                  : 'bg-stone-100 active:bg-stone-200 text-stone-500'
              }`}
              title="ปิดเสียง"
            >
              <VolumeX className="w-5 h-5" />
            </button>

            {[1, 2, 3, 4, 5].map((lvl) => (
              <button
                key={lvl}
                onClick={() => handleSelectVolume(lvl)}
                className={`py-3.5 rounded-2xl font-extrabold text-xl transition-all cursor-pointer ${
                  volLevel === lvl
                    ? 'bg-indigo-600 text-white shadow-md shadow-indigo-600/30 scale-105'
                    : 'bg-stone-100 active:bg-stone-200 text-stone-700'
                }`}
              >
                {lvl}
              </button>
            ))}
          </div>

          <div className="flex justify-between text-sm font-semibold text-stone-400 px-1 pt-0.5">
            <span>ปิดเสียง / เบา</span>
            <span>ดังสุด</span>
          </div>
        </div>

      </main>
    </div>
  );
}
